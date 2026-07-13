#include "CanvasDecoder.h"

#include "Document.h"
#include "CanvasDocumentDecoder.h"
#include "CanvasSchema.h"
#include "DiagnosticsMacros.h"
#include "KiwiByteReader.h"
#include "Memory.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

#include <zlib.h>
#include <zstd.h>

#include <kiwi/kiwi.h>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        constexpr char KIWI_HEADER[] = "fig-kiwi";

        struct CanvasChunkDesc
        {
            std::size_t offset = 0;
            std::size_t size = 0;
        };

        using CanvasChunkVector = FigmaVector<CanvasChunkDesc>;
        //////////////////////////////////////////////////////////////////////////
        static void * zlibAlloc( void * _opaque, unsigned int _items, unsigned int _size )
        {
            FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>( _opaque );
            const std::size_t itemCount = _items;
            const std::size_t itemSize = _size;

            if( itemSize != 0 && itemCount > std::numeric_limits<std::size_t>::max() / itemSize )
            {
                return nullptr;
            }

            return allocateMemoryBlock( memory, itemCount * itemSize );
        }
        //////////////////////////////////////////////////////////////////////////
        static void zlibFree( void * _opaque, void * _address )
        {
            if( _address == nullptr )
            {
                return;
            }

            FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>( _opaque );
            deallocateMemoryBlock( memory, _address );
        }
        //////////////////////////////////////////////////////////////////////////
        static std::uint32_t readLittleEndian32( const std::uint8_t * _bytes )
        {
            return static_cast<std::uint32_t>( _bytes[0] ) | ( static_cast<std::uint32_t>( _bytes[1] ) << 8 ) | ( static_cast<std::uint32_t>( _bytes[2] ) << 16 ) |
                   ( static_cast<std::uint32_t>( _bytes[3] ) << 24 );
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString makeString( FigmaMemoryResource * _memory, FigmaStringView _value )
        {
            return FigmaString( _value.begin(), _value.end(), _memory );
        }
        //////////////////////////////////////////////////////////////////////////
        static bool inflateRaw( FigmaMemoryResource * _memory, const std::uint8_t * _data, std::size_t _size, FigmaByteBuffer * const _out )
        {
            _out->clear();

            z_stream stream;
            std::memset( &stream, 0, sizeof( stream ) );
            stream.zalloc = &zlibAlloc;
            stream.zfree = &zlibFree;
            stream.opaque = _memory;
            stream.next_in = const_cast<Bytef *>( reinterpret_cast<const Bytef *>( _data ) );
            stream.avail_in = static_cast<uInt>( _size );

            if( inflateInit2( &stream, -MAX_WBITS ) != Z_OK )
            {
                return false;
            }

            int result = Z_OK;
            do
            {
                const std::size_t oldSize = _out->size();
                _out->resize( oldSize + 65536 );
                stream.next_out = reinterpret_cast<Bytef *>( _out->data() + oldSize );
                stream.avail_out = 65536;

                result = inflate( &stream, Z_NO_FLUSH );
                if( result != Z_OK && result != Z_STREAM_END )
                {
                    inflateEnd( &stream );
                    return false;
                }

                _out->resize( oldSize + ( 65536 - stream.avail_out ) );
            } while( result != Z_STREAM_END );

            inflateEnd( &stream );
            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool decompressZstd( const std::uint8_t * _data, std::size_t _size, FigmaByteBuffer * const _out )
        {
            const unsigned long long contentSize = ZSTD_getFrameContentSize( _data, _size );
            if( contentSize == ZSTD_CONTENTSIZE_ERROR || contentSize == ZSTD_CONTENTSIZE_UNKNOWN ||
                contentSize > static_cast<unsigned long long>( std::numeric_limits<std::size_t>::max() ) )
            {
                return false;
            }

            _out->resize( static_cast<std::size_t>( contentSize ) );
            const std::size_t result = ZSTD_decompress( _out->data(), _out->size(), _data, _size );
            if( ZSTD_isError( result ) != 0 || result != _out->size() )
            {
                _out->clear();
                return false;
            }
            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool decompressChunk( FigmaMemoryResource * _memory, const std::uint8_t * _data, std::size_t _size, FigmaByteBuffer * const _out )
        {
            if( inflateRaw( _memory, _data, _size, _out ) == true )
            {
                return true;
            }

            return decompressZstd( _data, _size, _out );
        }
        //////////////////////////////////////////////////////////////////////////
        static bool readChunks( const FigmaByteBuffer & _bytes, CanvasChunkVector * const _chunks )
        {
            if( _bytes.size() < 16 || std::memcmp( _bytes.data(), KIWI_HEADER, sizeof( KIWI_HEADER ) - 1 ) != 0 )
            {
                return false;
            }

            std::size_t offset = 12;
            while( offset < _bytes.size() )
            {
                if( _bytes.size() - offset < 4 )
                {
                    return false;
                }

                const std::uint32_t chunkSize = readLittleEndian32( _bytes.data() + offset );
                offset += 4;
                if( chunkSize > _bytes.size() - offset )
                {
                    return false;
                }

                CanvasChunkDesc chunk;
                chunk.offset = offset;
                chunk.size = chunkSize;
                _chunks->emplace_back( chunk );
                offset += chunkSize;
            }

            return _chunks->size() >= 2;
        }
        //////////////////////////////////////////////////////////////////////////
        static EKiwiDefinitionKind readDefinitionKind( std::uint8_t _value )
        {
            switch( _value )
            {
            case 0:
                return EKiwiDefinitionKind::Enum;
            case 1:
                return EKiwiDefinitionKind::Struct;
            case 2:
                return EKiwiDefinitionKind::Message;
            default:
                throw std::runtime_error( "Unknown Kiwi definition kind" );
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void decodeBinarySchema( FigmaMemoryResource * _memory, const FigmaByteBuffer & _bytes, KiwiSchemaDesc * const _schema )
        {
            static constexpr FigmaStringView PrimitiveTypes[] = { "bool", "byte", "int", "uint", "float", "string", "int64", "uint64" };

            KiwiByteReader reader( _bytes.data(), _bytes.size() );
            const std::uint32_t definitionCount = reader.readVarUint();
            _schema->definitions.reserve( definitionCount );

            struct RawFieldDesc
            {
                explicit RawFieldDesc( FigmaMemoryResource * _memory )
                    : name( _memory )
                {
                }

                FigmaString name;
                std::int32_t type = 0;
                bool array = false;
                std::uint32_t value = 0;
            };

            using RawFieldVector = FigmaVector<RawFieldDesc>;
            using RawFieldVectorVector = FigmaVector<RawFieldVector>;

            RawFieldVectorVector rawFields( _memory );
            rawFields.reserve( definitionCount );

            for( std::uint32_t definitionIndex = 0; definitionIndex != definitionCount; ++definitionIndex )
            {
                KiwiDefinitionDesc definition( _memory );
                definition.name = reader.readString( _memory );
                definition.kind = readDefinitionKind( reader.readByte() );

                const std::uint32_t fieldCount = reader.readVarUint();
                RawFieldVector fields( _memory );
                fields.reserve( fieldCount );
                definition.fields.reserve( fieldCount );

                for( std::uint32_t fieldIndex = 0; fieldIndex != fieldCount; ++fieldIndex )
                {
                    RawFieldDesc rawField( _memory );
                    rawField.name = reader.readString( _memory );
                    rawField.type = reader.readVarInt();
                    rawField.array = ( reader.readByte() & 1u ) != 0u;
                    rawField.value = reader.readVarUint();

                    KiwiFieldDesc field( _memory );
                    field.name = rawField.name;
                    field.array = rawField.array;
                    field.value = rawField.value;
                    definition.fields.emplace_back( std::move( field ) );
                    fields.emplace_back( std::move( rawField ) );
                }

                _schema->definitions.emplace_back( std::move( definition ) );
                rawFields.emplace_back( std::move( fields ) );
            }

            const std::size_t definitionSize = _schema->definitions.size();
            for( std::size_t definitionIndex = 0; definitionIndex != definitionSize; ++definitionIndex )
            {
                KiwiDefinitionDesc & definition = _schema->definitions[definitionIndex];
                const RawFieldVector & fields = rawFields[definitionIndex];

                const std::size_t fieldSize = definition.fields.size();
                for( std::size_t fieldIndex = 0; fieldIndex != fieldSize; ++fieldIndex )
                {
                    const RawFieldDesc & rawField = fields[fieldIndex];
                    KiwiFieldDesc & field = definition.fields[fieldIndex];

                    if( definition.kind == EKiwiDefinitionKind::Enum )
                    {
                        field.type = "enum";
                        continue;
                    }

                    if( rawField.type < 0 )
                    {
                        const std::size_t primitiveIndex = static_cast<std::size_t>( ~rawField.type );
                        if( primitiveIndex >= std::size( PrimitiveTypes ) )
                        {
                            throw std::runtime_error( "Unknown Kiwi primitive type" );
                        }

                        field.type = makeString( _memory, PrimitiveTypes[primitiveIndex] );
                    }
                    else
                    {
                        const std::size_t typeIndex = static_cast<std::size_t>( rawField.type );
                        if( typeIndex >= definitionSize )
                        {
                            throw std::runtime_error( "Unknown Kiwi schema type index" );
                        }

                        field.type = _schema->definitions[typeIndex].name;
                    }
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool decodeCanvas( RuntimeInterface * const _runtime, const FigmaByteBuffer & _bytes, Document * const _document, DiagnosticsInterface * const _diagnostics )
    {
        if( _runtime == nullptr || _document == nullptr )
        {
            return false;
        }

        FigmaMemoryResource * memory = _runtime->getMemory();

        try
        {
            Detail::CanvasChunkVector chunks( memory );
            if( Detail::readChunks( _bytes, &chunks ) == false )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "fig_canvas_decode_failed", "Unable to parse fig-kiwi chunk table" );
                }

                return false;
            }

            FigmaByteBuffer encodedSchema( memory );
            FigmaByteBuffer encodedData( memory );
            const Detail::CanvasChunkDesc & schemaChunk = chunks[0];
            const Detail::CanvasChunkDesc & dataChunk = chunks[1];
            if( Detail::decompressChunk( memory, _bytes.data() + schemaChunk.offset, schemaChunk.size, &encodedSchema ) == false ||
                Detail::decompressChunk( memory, _bytes.data() + dataChunk.offset, dataChunk.size, &encodedData ) == false )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "fig_canvas_inflate_failed", "Unable to inflate fig-kiwi schema or scene chunk" );
                }

                return false;
            }

            KiwiSchemaDesc schema( memory );
            Detail::decodeBinarySchema( memory, encodedSchema, &schema );

            KiwiByteReader reader( encodedData.data(), encodedData.size() );
            CanvasDocumentDecoder decoder( memory, schema );
            if( decoder.decode( reader ) == false )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "fig_canvas_decode_failed", "Unable to decode fig-kiwi scene graph" );
                }

                return false;
            }

            _document->m_canvasRoot = decoder.takeCanvasRoot();
            _document->m_prototypeStartNodeId = decoder.takePrototypeStartNodeId();
            _document->m_hasCanvasRoot = true;

            return true;
        }
        catch( const std::exception & _exception )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Warning, "fig_canvas_decode_failed", _exception.what() );
            }

            return false;
        }
    }
    //////////////////////////////////////////////////////////////////////////
}
