#include "DocumentLoader.h"

#include "Document.h"
#include "CanvasDecoder.h"
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
        constexpr char KiwiPrefix[] = "fig-kiwi";
        constexpr char SupportedKiwiVersion = 'j';
        //////////////////////////////////////////////////////////////////////////
        static FigmaString makeString(FigmaMemoryResource * _memory, FigmaStringView _value)
        {
            return FigmaString(_value.begin(), _value.end(), _memory);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool startsWith(FigmaStringView _value, FigmaStringView _prefix)
        {
            return _value.size() >= _prefix.size() && _value.substr(0, _prefix.size()) == _prefix;
        }
        //////////////////////////////////////////////////////////////////////////
        static std::uint32_t readBigEndian32(const std::uint8_t * _bytes)
        {
            return (static_cast<std::uint32_t>(_bytes[0]) << 24) |
                (static_cast<std::uint32_t>(_bytes[1]) << 16) |
                (static_cast<std::uint32_t>(_bytes[2]) << 8) |
                static_cast<std::uint32_t>(_bytes[3]);
        }
        //////////////////////////////////////////////////////////////////////////
        static void fillImageMetadata(AssetDesc * const _asset)
        {
            if(_asset == nullptr)
            {
                return;
            }

            const FigmaByteBuffer & bytes = _asset->bytes;
            if(bytes.size() >= 33 &&
                std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0 &&
                readBigEndian32(bytes.data() + 8) == 13 &&
                std::memcmp(bytes.data() + 12, "IHDR", 4) == 0)
            {
                _asset->width = readBigEndian32(bytes.data() + 16);
                _asset->height = readBigEndian32(bytes.data() + 20);
                _asset->colorType = bytes[25];
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString detectMime(FigmaMemoryResource * _memory, const FigmaByteBuffer & _bytes)
        {
            if(_bytes.size() >= 8 && std::memcmp(_bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0)
            {
                return makeString(_memory, "image/png");
            }

            if(_bytes.size() >= 3 && _bytes[0] == 0xff && _bytes[1] == 0xd8 && _bytes[2] == 0xff)
            {
                return makeString(_memory, "image/jpeg");
            }

            if(_bytes.size() >= 12 && std::memcmp(_bytes.data(), "RIFF", 4) == 0 && std::memcmp(_bytes.data() + 8, "WEBP", 4) == 0)
            {
                return makeString(_memory, "image/webp");
            }

            if(_bytes.size() >= 6 && (std::memcmp(_bytes.data(), "GIF87a", 6) == 0 || std::memcmp(_bytes.data(), "GIF89a", 6) == 0))
            {
                return makeString(_memory, "image/gif");
            }

            return makeString(_memory, "application/octet-stream");
        }
        //////////////////////////////////////////////////////////////////////////
        struct MetaInfo
        {
            explicit MetaInfo(FigmaMemoryResource * _memory)
                : fileName(_memory)
            {
            }

            FigmaString fileName;
            Rectf renderCoordinates{0.0f, 0.0f, 1024.0f, 768.0f};
            Vec2f thumbnailSize{0.0f, 0.0f};
        };
        //////////////////////////////////////////////////////////////////////////
        static MetaInfo parseMetaJson(FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, FigmaStringView _metaJson)
        {
            MetaInfo meta(_memory);

            JsonDocument json;
            if(parseJson(_memory, _metaJson, _diagnostics, &json) != EResult::Ok)
            {
                return meta;
            }

            const js_element_t * root = json.getRoot();
            const FigmaStringView fileName = jsonString(jsonMember(root, "file_name"));
            if(fileName.empty() == false)
            {
                meta.fileName = makeString(_memory, fileName);
            }

            const js_element_t * clientMeta = jsonMember(root, "client_meta");
            const js_element_t * renderCoordinates = jsonMember(clientMeta, "render_coordinates");
            meta.renderCoordinates.x = static_cast<float>(jsonNumber(jsonMember(renderCoordinates, "x"), 0.0));
            meta.renderCoordinates.y = static_cast<float>(jsonNumber(jsonMember(renderCoordinates, "y"), 0.0));
            meta.renderCoordinates.w = static_cast<float>(jsonNumber(jsonMember(renderCoordinates, "width"), 1024.0));
            meta.renderCoordinates.h = static_cast<float>(jsonNumber(jsonMember(renderCoordinates, "height"), 768.0));

            const js_element_t * thumbnailSize = jsonMember(clientMeta, "thumbnail_size");
            meta.thumbnailSize.x = static_cast<float>(jsonNumber(jsonMember(thumbnailSize, "width"), 0.0));
            meta.thumbnailSize.y = static_cast<float>(jsonNumber(jsonMember(thumbnailSize, "height"), 0.0));

            return meta;
        }
        //////////////////////////////////////////////////////////////////////////
        static EBindingProperty parseBindingProperty(FigmaStringView _value)
        {
            if(_value == "visible")
            {
                return EBindingProperty::Visible;
            }

            if(_value == "enabled")
            {
                return EBindingProperty::Enabled;
            }

            if(_value == "selected")
            {
                return EBindingProperty::Selected;
            }

            if(_value == "image")
            {
                return EBindingProperty::Image;
            }

            return EBindingProperty::Text;
        }
        //////////////////////////////////////////////////////////////////////////
        static void parseBindingArray(FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, const js_element_t * _array, BindingVector * const _bindings)
        {
            if(_array == nullptr || js_is_array(_array) != JS_TRUE)
            {
                return;
            }

            const std::size_t count = js_array_size(_array);
            for(std::size_t index = 0; index != count; ++index)
            {
                const js_element_t * item = jsonIndex(_array, index);
                const FigmaStringView nodeId = jsonString(jsonMember(item, "nodeId"));
                const FigmaStringView key = jsonString(jsonMember(item, "key"));
                if(nodeId.empty() == true || key.empty() == true)
                {
                    if(_diagnostics != nullptr)
                    {
                        _diagnostics->add(EDiagnosticSeverity::Warning, "ux_binding_skipped", "Binding entry requires nodeId and key");
                    }
                    continue;
                }

                BindingDesc itemDesc(_memory);
                itemDesc.nodeId = makeString(_memory, nodeId);
                itemDesc.key = makeString(_memory, key);
                itemDesc.property = parseBindingProperty(jsonString(jsonMember(item, "property"), "text"));
                _bindings->emplace_back(std::move(itemDesc));
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void parseActionArray(FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, const js_element_t * _array, ActionVector * const _actions)
        {
            if(_array == nullptr || js_is_array(_array) != JS_TRUE)
            {
                return;
            }

            const std::size_t count = js_array_size(_array);
            for(std::size_t index = 0; index != count; ++index)
            {
                const js_element_t * item = jsonIndex(_array, index);
                const FigmaStringView nodeId = jsonString(jsonMember(item, "nodeId"));
                const FigmaStringView actionId = jsonString(jsonMember(item, "actionId"));
                if(nodeId.empty() == true || actionId.empty() == true)
                {
                    if(_diagnostics != nullptr)
                    {
                        _diagnostics->add(EDiagnosticSeverity::Warning, "ux_action_skipped", "Action entry requires nodeId and actionId");
                    }
                    continue;
                }

                ActionDesc action(_memory);
                action.nodeId = makeString(_memory, nodeId);
                action.actionId = makeString(_memory, actionId);
                action.targetFrameId = makeString(_memory, jsonString(jsonMember(item, "targetFrameId")));
                _actions->emplace_back(std::move(action));
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        struct DocumentDestroyDeleter
        {
            void operator()(Document * const _document) const
            {
                if(_document != nullptr)
                {
                    _document->destroy();
                }
            }
        };
    }
    //////////////////////////////////////////////////////////////////////////
    EResult loadDocumentFromArchiveDataImpl(RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document)
    {
        if(_runtime == nullptr || _data == nullptr || _size == 0 || _document == nullptr)
        {
            return EResult::InvalidArgument;
        }

        *_document = nullptr;

        FigmaMemoryResource * memory = _runtime->getMemory();
        std::unique_ptr<Document, Detail::DocumentDestroyDeleter> documentHolder;
        try
        {
            documentHolder.reset(new Document(_runtime, memory));
        }
        catch(const std::bad_alloc &)
        {
            return EResult::OutOfMemory;
        }

        Document * const document = documentHolder.get();
        document->m_path = Detail::makeString(memory, _options.sourceName);
        document->m_renderCoordinates = {0.0f, 0.0f, 1024.0f, 768.0f};

        ZipArchive archive(_runtime);
        EResult result = archive.open(_data, _size, &document->m_diagnostics);
        if(result != EResult::Ok)
        {
            return result;
        }

        FigmaByteBuffer metaBytes(memory);
        if(archive.extractFile("meta.json", &metaBytes, &document->m_diagnostics) == false)
        {
            return EResult::MissingEntry;
        }

        const Detail::MetaInfo meta = Detail::parseMetaJson(memory, &document->m_diagnostics, FigmaStringView(reinterpret_cast<const char *>(metaBytes.data()), metaBytes.size()));
        document->m_fileName = meta.fileName;
        document->m_renderCoordinates = meta.renderCoordinates;
        document->m_thumbnailSize = meta.thumbnailSize;

        FigmaByteBuffer thumbnailBytes(memory);
        if(archive.extractFile("thumbnail.png", &thumbnailBytes, nullptr) == true)
        {
            AssetDesc thumbnail(memory);
            thumbnail.id = "thumbnail.png";
            thumbnail.path = "thumbnail.png";
            thumbnail.mime = "image/png";
            thumbnail.bytes = std::move(thumbnailBytes);
            Detail::fillImageMetadata(&thumbnail);
            document->m_assets.emplace_back(std::move(thumbnail));
        }
        else
        {
            document->m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_thumbnail_missing", "thumbnail.png is missing");
        }

        if(_options.extractImageAssets == true)
        {
            const std::size_t fileCount = archive.getFileCount();
            for(std::size_t index = 0; index != fileCount; ++index)
            {
                if(archive.isDirectory(index) == true)
                {
                    continue;
                }

                FigmaString name(memory);
                if(archive.getFileName(index, &name) == false || Detail::startsWith(name, "images/") == false || name == "images/")
                {
                    continue;
                }

                FigmaByteBuffer imageBytes(memory);
                if(archive.extractFileByIndex(index, &imageBytes, &document->m_diagnostics) == false)
                {
                    continue;
                }

                AssetDesc asset(memory);
                asset.id = FigmaString(name.substr(7), memory);
                asset.path = name;
                asset.mime = Detail::detectMime(memory, imageBytes);
                asset.bytes = std::move(imageBytes);
                Detail::fillImageMetadata(&asset);
                document->m_assets.emplace_back(std::move(asset));
            }
        }

        FigmaByteBuffer canvasBytes(memory);
        if(archive.extractFile("canvas.fig", &canvasBytes, &document->m_diagnostics) == false)
        {
            return EResult::MissingEntry;
        }

        if(canvasBytes.size() < 9 || std::memcmp(canvasBytes.data(), Detail::KiwiPrefix, sizeof(Detail::KiwiPrefix) - 1) != 0)
        {
            document->m_diagnostics.add(EDiagnosticSeverity::Error, "fig_canvas_bad_magic", "canvas.fig does not start with fig-kiwi");
            return EResult::UnsupportedFormat;
        }

        document->m_canvasVersion = static_cast<char>(canvasBytes[sizeof(Detail::KiwiPrefix) - 1]);
        if(document->m_canvasVersion != Detail::SupportedKiwiVersion)
        {
            document->m_diagnostics.add(EDiagnosticSeverity::Error, "fig_canvas_unsupported_version", "canvas.fig uses an unsupported fig-kiwi revision");
            return EResult::UnsupportedFormat;
        }

        if(_options.keepCanvasBytes == true)
        {
            document->m_canvasBytes = canvasBytes;
        }

        if(decodeCanvas(_runtime, canvasBytes, document, &document->m_diagnostics) == false)
        {
            document->m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_canvas_decoder_unsupported", "Binary fig-kiwi canvas decoding is not available for this file; render commands will be skipped until the required scene data is decoded");
        }

        if(document->m_fileName.empty() == true)
        {
            document->m_fileName = document->m_path;
        }

        *_document = documentHolder.release();

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Document::loadUX(FigmaStringView _data)
    {
        if(_data.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        JsonDocument json;
        EResult result = parseJson(this->getMemory(), _data, this->getDiagnostics(), &json);
        if(result != EResult::Ok)
        {
            return result;
        }

        const js_element_t * root = json.getRoot();
        Detail::parseBindingArray(this->getMemory(), &this->m_diagnostics, jsonMember(root, "bindings"), &this->m_bindings);
        Detail::parseActionArray(this->getMemory(), &this->m_diagnostics, jsonMember(root, "actions"), &this->m_actions);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}
