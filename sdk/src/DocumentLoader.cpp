#include "DocumentLoader.h"

#include "Document.h"
#include "CanvasDecoder.h"
#include "DiagnosticsMacros.h"
#include "JsonUtils.h"
#include "ZipArchive.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        constexpr char KIWI_PREFIX[] = "fig-kiwi";
        constexpr char SUPPORTED_KIWI_VERSION = 'j';
        //////////////////////////////////////////////////////////////////////////
        static FigmaString makeString( FigmaMemoryResource * _memory, FigmaStringView _value )
        {
            return FigmaString( _value.begin(), _value.end(), _memory );
        }
        //////////////////////////////////////////////////////////////////////////
        static bool startsWith( FigmaStringView _value, FigmaStringView _prefix )
        {
            return _value.size() >= _prefix.size() && _value.substr( 0, _prefix.size() ) == _prefix;
        }
        //////////////////////////////////////////////////////////////////////////
        static std::uint32_t readBigEndian32( const std::uint8_t * _bytes )
        {
            return ( static_cast<std::uint32_t>( _bytes[0] ) << 24 ) | ( static_cast<std::uint32_t>( _bytes[1] ) << 16 ) | ( static_cast<std::uint32_t>( _bytes[2] ) << 8 ) |
                   static_cast<std::uint32_t>( _bytes[3] );
        }
        //////////////////////////////////////////////////////////////////////////
        static void fillImageMetadata( AssetDesc * const _asset )
        {
            if( _asset == nullptr )
            {
                return;
            }

            const FigmaByteBuffer & bytes = _asset->bytes;
            if( bytes.size() >= 33 && std::memcmp( bytes.data(), "\x89PNG\r\n\x1a\n", 8 ) == 0 && readBigEndian32( bytes.data() + 8 ) == 13 &&
                std::memcmp( bytes.data() + 12, "IHDR", 4 ) == 0 )
            {
                _asset->width = readBigEndian32( bytes.data() + 16 );
                _asset->height = readBigEndian32( bytes.data() + 20 );
                _asset->colorType = bytes[25];
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString detectMime( FigmaMemoryResource * _memory, const FigmaByteBuffer & _bytes )
        {
            if( _bytes.size() >= 8 && std::memcmp( _bytes.data(), "\x89PNG\r\n\x1a\n", 8 ) == 0 )
            {
                return makeString( _memory, "image/png" );
            }

            if( _bytes.size() >= 3 && _bytes[0] == 0xff && _bytes[1] == 0xd8 && _bytes[2] == 0xff )
            {
                return makeString( _memory, "image/jpeg" );
            }

            if( _bytes.size() >= 12 && std::memcmp( _bytes.data(), "RIFF", 4 ) == 0 && std::memcmp( _bytes.data() + 8, "WEBP", 4 ) == 0 )
            {
                return makeString( _memory, "image/webp" );
            }

            if( _bytes.size() >= 6 && ( std::memcmp( _bytes.data(), "GIF87a", 6 ) == 0 || std::memcmp( _bytes.data(), "GIF89a", 6 ) == 0 ) )
            {
                return makeString( _memory, "image/gif" );
            }

            return makeString( _memory, "application/octet-stream" );
        }
        //////////////////////////////////////////////////////////////////////////
        struct MetaInfo
        {
            explicit MetaInfo( FigmaMemoryResource * _memory )
                : fileName( _memory )
            {
            }

            FigmaString fileName;
            Rectf renderCoordinates{ 0.0f, 0.0f, 1024.0f, 768.0f };
            Vec2f thumbnailSize{ 0.0f, 0.0f };
        };
        //////////////////////////////////////////////////////////////////////////
        static EResult parseMetaJson( FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, FigmaStringView _metaJson, MetaInfo * const _meta )
        {
            if( _memory == nullptr || _meta == nullptr )
            {
                return EResult::InvalidArgument;
            }

            JsonDocument json;
            const EResult result = parseJson( _memory, _metaJson, _diagnostics, &json );
            if( result != EResult::Ok )
            {
                return result;
            }

            const js_element_t * root = json.getRoot();
            if( root == nullptr || js_is_object( root ) != JS_TRUE )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_meta_invalid", "meta.json root must be a JSON object" );
                }

                return EResult::ParseFailed;
            }

            const FigmaStringView fileName = jsonString( jsonMember( root, "file_name" ) );
            if( fileName.empty() == false )
            {
                _meta->fileName = makeString( _memory, fileName );
            }

            const js_element_t * clientMeta = jsonMember( root, "client_meta" );
            const js_element_t * renderCoordinates = jsonMember( clientMeta, "render_coordinates" );
            _meta->renderCoordinates.x = static_cast<float>( jsonNumber( jsonMember( renderCoordinates, "x" ), 0.0 ) );
            _meta->renderCoordinates.y = static_cast<float>( jsonNumber( jsonMember( renderCoordinates, "y" ), 0.0 ) );
            _meta->renderCoordinates.w = static_cast<float>( jsonNumber( jsonMember( renderCoordinates, "width" ), 1024.0 ) );
            _meta->renderCoordinates.h = static_cast<float>( jsonNumber( jsonMember( renderCoordinates, "height" ), 768.0 ) );

            const js_element_t * thumbnailSize = jsonMember( clientMeta, "thumbnail_size" );
            _meta->thumbnailSize.x = static_cast<float>( jsonNumber( jsonMember( thumbnailSize, "width" ), 0.0 ) );
            _meta->thumbnailSize.y = static_cast<float>( jsonNumber( jsonMember( thumbnailSize, "height" ), 0.0 ) );

            return EResult::Ok;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool parseBindingProperty( FigmaStringView _value, EBindingProperty * const _property )
        {
            if( _property == nullptr )
            {
                return false;
            }

            if( _value.empty() == true || _value == "text" )
            {
                *_property = EBindingProperty::Text;
                return true;
            }

            if( _value == "visible" )
            {
                *_property = EBindingProperty::Visible;
                return true;
            }

            if( _value == "enabled" )
            {
                *_property = EBindingProperty::Enabled;
                return true;
            }

            if( _value == "selected" )
            {
                *_property = EBindingProperty::Selected;
                return true;
            }

            if( _value == "image" )
            {
                *_property = EBindingProperty::Image;
                return true;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static void parseBindingArray( FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, const js_element_t * _array, BindingVector * const _bindings )
        {
            if( _array == nullptr || js_is_array( _array ) != JS_TRUE )
            {
                return;
            }

            const std::size_t count = js_array_size( _array );
            for( std::size_t index = 0; index != count; ++index )
            {
                const js_element_t * item = jsonIndex( _array, index );
                const FigmaStringView nodeId = jsonString( jsonMember( item, "nodeId" ) );
                const FigmaStringView key = jsonString( jsonMember( item, "key" ) );
                if( nodeId.empty() == true || key.empty() == true )
                {
                    if( _diagnostics != nullptr )
                    {
                        FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "ux_binding_skipped", "Binding entry requires nodeId and key" );
                    }
                    continue;
                }

                BindingDesc itemDesc( _memory );
                itemDesc.nodeId = makeString( _memory, nodeId );
                itemDesc.key = makeString( _memory, key );

                const FigmaStringView property = jsonString( jsonMember( item, "property" ), "text" );
                if( parseBindingProperty( property, &itemDesc.property ) == false )
                {
                    if( _diagnostics != nullptr )
                    {
                        FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "ux_binding_property_unsupported", "Binding entry uses an unsupported property", itemDesc.nodeId.c_str() );
                    }

                    continue;
                }

                _bindings->emplace_back( std::move( itemDesc ) );
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void parseActionArray( FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, const js_element_t * _array, ActionVector * const _actions )
        {
            if( _array == nullptr || js_is_array( _array ) != JS_TRUE )
            {
                return;
            }

            const std::size_t count = js_array_size( _array );
            for( std::size_t index = 0; index != count; ++index )
            {
                const js_element_t * item = jsonIndex( _array, index );
                const FigmaStringView nodeId = jsonString( jsonMember( item, "nodeId" ) );
                const FigmaStringView actionId = jsonString( jsonMember( item, "actionId" ) );
                if( nodeId.empty() == true || actionId.empty() == true )
                {
                    if( _diagnostics != nullptr )
                    {
                        FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "ux_action_skipped", "Action entry requires nodeId and actionId" );
                    }
                    continue;
                }

                ActionDesc action( _memory );
                action.nodeId = makeString( _memory, nodeId );
                action.actionId = makeString( _memory, actionId );
                action.targetFrameId = makeString( _memory, jsonString( jsonMember( item, "targetFrameId" ) ) );

                const FigmaStringView trigger = jsonString( jsonMember( item, "trigger" ), "click" );
                if( trigger == "hover" || trigger == "hoverEnter" )
                {
                    action.eventType = EPrototypeEventType::HoverEnter;
                }
                else if( trigger == "hoverLeave" )
                {
                    action.eventType = EPrototypeEventType::HoverLeave;
                }
                else if( trigger == "press" )
                {
                    action.eventType = EPrototypeEventType::Press;
                }
                else if( trigger == "pointerDown" )
                {
                    action.eventType = EPrototypeEventType::PointerDown;
                }
                else if( trigger == "pointerUp" )
                {
                    action.eventType = EPrototypeEventType::PointerUp;
                }
                else if( trigger == "keyDown" )
                {
                    action.eventType = EPrototypeEventType::KeyDown;
                    action.keyCode = static_cast<std::uint32_t>( jsonNumber( jsonMember( item, "keyCode" ), 0.0 ) );
                }
                else if( trigger == "click" )
                {
                    action.eventType = EPrototypeEventType::Click;
                }
                else
                {
                    action.eventType = EPrototypeEventType::Unsupported;
                    if( _diagnostics != nullptr )
                    {
                        FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "ux_action_trigger_unsupported", "Action entry has an unsupported trigger" );
                    }
                }

                _actions->emplace_back( std::move( action ) );
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        struct DocumentDestroyDeleter
        {
            void operator()( Document * const _document ) const
            {
                if( _document != nullptr )
                {
                    _document->destroy();
                }
            }
        };

        using DocumentHolder = std::unique_ptr<Document, DocumentDestroyDeleter>;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult loadDocumentFromArchiveData(
        RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document )
    {
        if( _runtime == nullptr || _data == nullptr || _size == 0 || _document == nullptr )
        {
            return EResult::InvalidArgument;
        }

        *_document = nullptr;

        FigmaMemoryResource * memory = _runtime->getMemory();
        Detail::DocumentHolder documentHolder;
        void * documentMemory = nullptr;

        try
        {
            documentMemory = memory->allocate( sizeof( Document ), alignof( Document ) );
            documentHolder.reset( new( documentMemory ) Document( _runtime, memory ) );
        }
        catch( const std::bad_alloc & )
        {
            if( documentMemory != nullptr )
            {
                memory->deallocate( documentMemory, sizeof( Document ), alignof( Document ) );
            }

            return EResult::OutOfMemory;
        }
        catch( ... )
        {
            if( documentMemory != nullptr )
            {
                memory->deallocate( documentMemory, sizeof( Document ), alignof( Document ) );
            }

            throw;
        }

        Document * const document = documentHolder.get();
        document->m_path = Detail::makeString( memory, _options.sourceName );
        document->m_renderCoordinates = { 0.0f, 0.0f, 1024.0f, 768.0f };

        ZipArchive archive( _runtime );
        EResult result = archive.open( _data, _size, &document->m_diagnostics );
        if( result != EResult::Ok )
        {
            return result;
        }

        FigmaByteBuffer metaBytes( memory );
        if( archive.extractFile( "meta.json", &metaBytes, &document->m_diagnostics ) == false )
        {
            return EResult::MissingEntry;
        }

        Detail::MetaInfo meta( memory );
        const FigmaStringView metaJson( reinterpret_cast<const char *>( metaBytes.data() ), metaBytes.size() );
        result = Detail::parseMetaJson( memory, &document->m_diagnostics, metaJson, &meta );
        if( result != EResult::Ok )
        {
            return result;
        }

        document->m_fileName = meta.fileName;
        document->m_renderCoordinates = meta.renderCoordinates;
        document->m_thumbnailSize = meta.thumbnailSize;

        FigmaByteBuffer thumbnailBytes( memory );
        if( archive.extractFile( "thumbnail.png", &thumbnailBytes, nullptr ) == true )
        {
            AssetDesc thumbnail( memory );
            thumbnail.id = "thumbnail.png";
            thumbnail.path = "thumbnail.png";
            thumbnail.mime = "image/png";
            thumbnail.bytes = std::move( thumbnailBytes );
            Detail::fillImageMetadata( &thumbnail );
            document->m_assets.emplace_back( std::move( thumbnail ) );
        }
        else
        {
            FIGMA_DIAGNOSTICS_ADD( document->m_diagnostics, EDiagnosticSeverity::Warning, "fig_thumbnail_missing", "thumbnail.png is missing" );
        }

        if( _options.extractImageAssets == true )
        {
            const std::size_t fileCount = archive.getFileCount();
            for( std::size_t index = 0; index != fileCount; ++index )
            {
                if( archive.isDirectory( index ) == true )
                {
                    continue;
                }

                FigmaString name( memory );
                if( archive.getFileName( index, &name ) == false || Detail::startsWith( name, "images/" ) == false || name == "images/" )
                {
                    continue;
                }

                FigmaByteBuffer imageBytes( memory );
                if( archive.extractFileByIndex( index, &imageBytes, &document->m_diagnostics ) == false )
                {
                    continue;
                }

                AssetDesc asset( memory );
                asset.id = FigmaString( name.substr( 7 ), memory );
                asset.path = name;
                asset.mime = Detail::detectMime( memory, imageBytes );
                asset.bytes = std::move( imageBytes );
                Detail::fillImageMetadata( &asset );
                document->m_assets.emplace_back( std::move( asset ) );
            }
        }

        FigmaByteBuffer canvasBytes( memory );
        if( archive.extractFile( "canvas.fig", &canvasBytes, &document->m_diagnostics ) == false )
        {
            return EResult::MissingEntry;
        }

        if( canvasBytes.size() < 9 || std::memcmp( canvasBytes.data(), Detail::KIWI_PREFIX, sizeof( Detail::KIWI_PREFIX ) - 1 ) != 0 )
        {
            FIGMA_DIAGNOSTICS_ADD( document->m_diagnostics, EDiagnosticSeverity::Error, "fig_canvas_bad_magic", "canvas.fig does not start with fig-kiwi" );
            return EResult::UnsupportedFormat;
        }

        document->m_canvasVersion = static_cast<char>( canvasBytes[sizeof( Detail::KIWI_PREFIX ) - 1] );
        if( document->m_canvasVersion != Detail::SUPPORTED_KIWI_VERSION )
        {
            FIGMA_DIAGNOSTICS_ADD( document->m_diagnostics, EDiagnosticSeverity::Error, "fig_canvas_unsupported_version", "canvas.fig uses an unsupported fig-kiwi revision" );
            return EResult::UnsupportedFormat;
        }

        if( _options.keepCanvasBytes == true )
        {
            document->m_canvasBytes = canvasBytes;
        }

        if( decodeCanvas( _runtime, canvasBytes, document, &document->m_diagnostics ) == false )
        {
            FIGMA_DIAGNOSTICS_ADD( document->m_diagnostics,
                EDiagnosticSeverity::Warning,
                "fig_canvas_decoder_unsupported",
                "Binary fig-kiwi canvas decoding is not available for this file; render commands will be skipped until the required scene data is decoded" );
        }

        if( document->m_fileName.empty() == true )
        {
            document->m_fileName = document->m_path;
        }

        *_document = documentHolder.release();

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult loadDocumentUX(
        FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, FigmaStringView _data, BindingVector * const _bindings, ActionVector * const _actions )
    {
        if( _memory == nullptr || _data.empty() == true || _bindings == nullptr || _actions == nullptr )
        {
            return EResult::InvalidArgument;
        }

        JsonDocument json;
        const EResult result = parseJson( _memory, _data, _diagnostics, &json );
        if( result != EResult::Ok )
        {
            return result;
        }

        const js_element_t * root = json.getRoot();
        if( root == nullptr || js_is_object( root ) != JS_TRUE )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "ux_root_invalid", "UX data root must be a JSON object" );
            }

            return EResult::ParseFailed;
        }

        const js_element_t * bindings = jsonMember( root, "bindings" );
        if( bindings != nullptr && js_is_array( bindings ) != JS_TRUE )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "ux_bindings_invalid", "UX bindings must be a JSON array" );
            }

            return EResult::ParseFailed;
        }

        const js_element_t * actions = jsonMember( root, "actions" );
        if( actions != nullptr && js_is_array( actions ) != JS_TRUE )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "ux_actions_invalid", "UX actions must be a JSON array" );
            }

            return EResult::ParseFailed;
        }

        Detail::parseBindingArray( _memory, _diagnostics, bindings, _bindings );
        Detail::parseActionArray( _memory, _diagnostics, actions, _actions );

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}
