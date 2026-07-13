#include "CanvasDocumentDecoder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        static std::uint32_t readLittleEndian32(const std::uint8_t * _bytes)
        {
            return static_cast<std::uint32_t>(_bytes[0]) |
                (static_cast<std::uint32_t>(_bytes[1]) << 8) |
                (static_cast<std::uint32_t>(_bytes[2]) << 16) |
                (static_cast<std::uint32_t>(_bytes[3]) << 24);
        }
        //////////////////////////////////////////////////////////////////////////
        static float readLittleEndianFloat32(const std::uint8_t * _bytes)
        {
            const std::uint32_t encoded = readLittleEndian32(_bytes);
            float value = 0.0f;
            std::memcpy(&value, &encoded, sizeof(value));
            return value;
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString guidToString(FigmaMemoryResource * _memory, std::uint32_t _sessionId, std::uint32_t _localId)
        {
            char buffer[64] = {'\0'};
            const int size = std::snprintf(buffer, sizeof(buffer), "%u:%u", _sessionId, _localId);
            return FigmaString(buffer, buffer + std::max(0, size), _memory);
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString bytesToHex(FigmaMemoryResource * _memory, const FigmaByteBuffer & _bytes)
        {
            constexpr char Hex[] = "0123456789abcdef";
            FigmaString result(_memory);
            result.reserve(_bytes.size() * 2);
            for(std::uint8_t value : _bytes)
            {
                result.push_back(Hex[(value >> 4) & 0x0f]);
                result.push_back(Hex[value & 0x0f]);
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString utf8Substring(FigmaMemoryResource * _memory, const FigmaString & _value, std::uint32_t _first, std::uint32_t _end)
        {
            std::size_t byteBegin = _value.size();
            std::size_t byteEnd = _value.size();
            std::uint32_t characterIndex = 0;

            const std::size_t valueSize = _value.size();
            for(std::size_t index = 0; index < valueSize;)
            {
                if(characterIndex == _first)
                {
                    byteBegin = index;
                }

                if(characterIndex == _end)
                {
                    byteEnd = index;
                    break;
                }

                const unsigned char byte = static_cast<unsigned char>(_value[index]);
                std::size_t advance = 1;
                if((byte & 0x80u) == 0u)
                {
                    advance = 1;
                }
                else if((byte & 0xe0u) == 0xc0u)
                {
                    advance = 2;
                }
                else if((byte & 0xf0u) == 0xe0u)
                {
                    advance = 3;
                }
                else if((byte & 0xf8u) == 0xf0u)
                {
                    advance = 4;
                }

                index = std::min(_value.size(), index + advance);
                ++characterIndex;
            }

            if(characterIndex == _first)
            {
                byteBegin = _value.size();
            }

            if(characterIndex == _end)
            {
                byteEnd = _value.size();
            }

            if(byteBegin > byteEnd)
            {
                byteBegin = byteEnd;
            }

            return FigmaString(_value.data() + byteBegin, _value.data() + byteEnd, _memory);
        }
        //////////////////////////////////////////////////////////////////////////
        static MatrixDesc multiply(const MatrixDesc & _a, const MatrixDesc & _b)
        {
            MatrixDesc result;
            result.m00 = _a.m00 * _b.m00 + _a.m01 * _b.m10;
            result.m01 = _a.m00 * _b.m01 + _a.m01 * _b.m11;
            result.m02 = _a.m00 * _b.m02 + _a.m01 * _b.m12 + _a.m02;
            result.m10 = _a.m10 * _b.m00 + _a.m11 * _b.m10;
            result.m11 = _a.m10 * _b.m01 + _a.m11 * _b.m11;
            result.m12 = _a.m10 * _b.m02 + _a.m11 * _b.m12 + _a.m12;
            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        static Vec2f transformPoint(const MatrixDesc & _matrix, float _x, float _y)
        {
            return {
                _matrix.m00 * _x + _matrix.m01 * _y + _matrix.m02,
                _matrix.m10 * _x + _matrix.m11 * _y + _matrix.m12
            };
        }
        //////////////////////////////////////////////////////////////////////////
        static Rectf transformedRect(const MatrixDesc & _matrix, const Vec2f & _size)
        {
            const Vec2f p0 = transformPoint(_matrix, 0.0f, 0.0f);
            const Vec2f p1 = transformPoint(_matrix, _size.x, 0.0f);
            const Vec2f p2 = transformPoint(_matrix, 0.0f, _size.y);
            const Vec2f p3 = transformPoint(_matrix, _size.x, _size.y);

            const float minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
            const float minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
            const float maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
            const float maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));

            return {minX, minY, maxX - minX, maxY - minY};
        }
        //////////////////////////////////////////////////////////////////////////
        static void assignTransformedQuad(const MatrixDesc & _matrix, const Vec2f & _size, Vec2f * const _quad)
        {
            _quad[0] = transformPoint(_matrix, 0.0f, 0.0f);
            _quad[1] = transformPoint(_matrix, _size.x, 0.0f);
            _quad[2] = transformPoint(_matrix, _size.x, _size.y);
            _quad[3] = transformPoint(_matrix, 0.0f, _size.y);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasDocumentDecoder::CanvasDocumentDecoder(FigmaMemoryResource * _memory, const KiwiSchemaDesc & _schema)
        : CanvasReader(_memory, _schema)
        , m_records(_memory)
        , m_nodeIndex(_memory)
        , m_canvasRoot(_memory)
        , m_prototypeStartNodeId(_memory)
        , m_blobs(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasDocumentDecoder::~CanvasDocumentDecoder()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    bool CanvasDocumentDecoder::decode(KiwiByteReader & _reader)
    {
        const KiwiDefinitionDesc * messageDefinition = findKiwiDefinition(m_schema, "Message");
        if(messageDefinition == nullptr)
        {
            return false;
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*messageDefinition, tag);
            if(field == nullptr)
            {
                return false;
            }

            if(field->name == "nodeChanges" && field->array == true)
            {
                const std::uint32_t count = _reader.readVarUint();
                m_records.reserve(count);
                for(std::uint32_t index = 0; index != count; ++index)
                {
                    m_records.emplace_back(m_memory);
                    this->decodeNodeChange(_reader, &m_records.back());
                }
            }
            else if(field->name == "blobs" && field->array == true)
            {
                this->decodeBlobArray(_reader);
            }
            else if(field->name == "blobBaseIndex")
            {
                m_blobBaseIndex = _reader.readVarUint();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        this->resolveGeometryBlobs();
        this->resolvePaintStyleReferences();
        this->resolvePathStylePaints();

        return this->buildDocumentTree();
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasNodeDesc CanvasDocumentDecoder::takeCanvasRoot()
    {
        return std::move(m_canvasRoot);
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasDocumentDecoder::takePrototypeStartNodeId()
    {
        return std::move(m_prototypeStartNodeId);
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasDocumentDecoder::decodeStyleId(KiwiByteReader & _reader)
    {
        FigmaString styleNodeId(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "StyleId");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing StyleId schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown StyleId field");
            }

            if(field->name == "guid")
            {
                styleNodeId = this->decodeGuid(_reader);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return styleNodeId;
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasDocumentDecoder::decodeGuid(KiwiByteReader & _reader)
    {
        std::uint32_t sessionId = 0;
        std::uint32_t localId = 0;

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "GUID");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing GUID schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.name == "sessionID")
            {
                sessionId = _reader.readVarUint();
            }
            else if(field.name == "localID")
            {
                localId = _reader.readVarUint();
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return Detail::guidToString(m_memory, sessionId, localId);
    }
    //////////////////////////////////////////////////////////////////////////
    Vec2f CanvasDocumentDecoder::decodeVector(KiwiByteReader & _reader)
    {
        Vec2f value{};

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Vector");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Vector schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.name == "x")
            {
                value.x = _reader.readVarFloat();
            }
            else if(field.name == "y")
            {
                value.y = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    MatrixDesc CanvasDocumentDecoder::decodeMatrix(KiwiByteReader & _reader)
    {
        MatrixDesc value;

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Matrix");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Matrix schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            float * target = nullptr;
            if(field.name == "m00")
            {
                target = &value.m00;
            }
            else if(field.name == "m01")
            {
                target = &value.m01;
            }
            else if(field.name == "m02")
            {
                target = &value.m02;
            }
            else if(field.name == "m10")
            {
                target = &value.m10;
            }
            else if(field.name == "m11")
            {
                target = &value.m11;
            }
            else if(field.name == "m12")
            {
                target = &value.m12;
            }

            if(target != nullptr)
            {
                *target = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    Colorf CanvasDocumentDecoder::decodeColor(KiwiByteReader & _reader)
    {
        Colorf value{1.0f, 1.0f, 1.0f, 1.0f};

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Color");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Color schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            float * target = nullptr;
            if(field.name == "r")
            {
                target = &value.r;
            }
            else if(field.name == "g")
            {
                target = &value.g;
            }
            else if(field.name == "b")
            {
                target = &value.b;
            }
            else if(field.name == "a")
            {
                target = &value.a;
            }

            if(target != nullptr)
            {
                *target = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    ParentIndexDesc CanvasDocumentDecoder::decodeParentIndex(KiwiByteReader & _reader)
    {
        ParentIndexDesc value(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "ParentIndex");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing ParentIndex schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.name == "guid")
            {
                value.id = this->decodeGuid(_reader);
            }
            else if(field.name == "position")
            {
                value.position = _reader.readString(m_memory);
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    FontNameDesc CanvasDocumentDecoder::decodeFontName(KiwiByteReader & _reader)
    {
        FontNameDesc value(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "FontName");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing FontName schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.name == "family")
            {
                value.family = _reader.readString(m_memory);
            }
            else if(field.name == "style")
            {
                value.style = _reader.readString(m_memory);
            }
            else if(field.name == "postscript")
            {
                value.postscript = _reader.readString(m_memory);
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    NumberDesc CanvasDocumentDecoder::decodeNumber(KiwiByteReader & _reader)
    {
        NumberDesc value;

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Number");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Number schema");
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.name == "value")
            {
                value.value = _reader.readVarFloat();
            }
            else if(field.name == "units")
            {
                value.percent = this->readEnum(_reader, field.type) == "PERCENT";
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeFloatArray(KiwiByteReader & _reader, CanvasFloatVector * const _values)
    {
        const std::uint32_t count = _reader.readVarUint();
        _values->reserve(_values->size() + count);
        for(std::uint32_t index = 0; index != count; ++index)
        {
            _values->emplace_back(_reader.readVarFloat());
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeBlobArray(KiwiByteReader & _reader)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Blob");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Blob schema");
        }

        const std::uint32_t count = _reader.readVarUint();
        m_blobs.reserve(count);
        for(std::uint32_t index = 0; index != count; ++index)
        {
            FigmaByteBuffer bytes(m_memory);
            for(const KiwiFieldDesc & field : definition->fields)
            {
                if(field.name == "bytes")
                {
                    bytes = _reader.readByteArray(m_memory);
                }
                else
                {
                    this->skipValue(_reader, field.type, field.array);
                }
            }

            m_blobs.emplace_back(std::move(bytes));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasPathDesc CanvasDocumentDecoder::decodePath(KiwiByteReader & _reader)
    {
        CanvasPathDesc path(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Path");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Path schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown Path field");
            }

            if(field->name == "windingRule")
            {
                path.windingRule = this->readEnum(_reader, field->type) == "ODD" ? ECanvasWindingRule::Odd : ECanvasWindingRule::NonZero;
            }
            else if(field->name == "commandsBlob")
            {
                path.commandsBlob = _reader.readVarUint();
            }
            else if(field->name == "styleID")
            {
                path.styleId = _reader.readVarUint();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return path;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodePathArray(KiwiByteReader & _reader, CanvasPathVector * const _paths)
    {
        const std::uint32_t count = _reader.readVarUint();
        _paths->reserve(_paths->size() + count);
        for(std::uint32_t index = 0; index != count; ++index)
        {
            _paths->emplace_back(this->decodePath(_reader));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool CanvasDocumentDecoder::readPathPoint(const FigmaByteBuffer & _blob, std::size_t * const _offset, Vec2f * const _point) const
    {
        if(_blob.size() - *_offset < 8)
        {
            return false;
        }

        _point->x = Detail::readLittleEndianFloat32(_blob.data() + *_offset + 0);
        _point->y = Detail::readLittleEndianFloat32(_blob.data() + *_offset + 4);
        *_offset += 8;
        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodePathCommands(const FigmaByteBuffer & _blob, CanvasPathDesc * const _path) const
    {
        _path->commands.clear();

        std::size_t offset = 0;
        while(offset < _blob.size())
        {
            const std::uint8_t opcode = _blob[offset++];
            CanvasPathCommandDesc command{};

            switch(opcode)
            {
            case 0:
                command.type = ECanvasPathCommandType::Close;
                break;
            case 1:
                command.type = ECanvasPathCommandType::MoveTo;
                if(this->readPathPoint(_blob, &offset, &command.p0) == false)
                {
                    return;
                }
                break;
            case 2:
                command.type = ECanvasPathCommandType::LineTo;
                if(this->readPathPoint(_blob, &offset, &command.p0) == false)
                {
                    return;
                }
                break;
            case 3:
                command.type = ECanvasPathCommandType::QuadraticTo;
                if(this->readPathPoint(_blob, &offset, &command.p0) == false || this->readPathPoint(_blob, &offset, &command.p1) == false)
                {
                    return;
                }
                break;
            case 4:
                command.type = ECanvasPathCommandType::CubicTo;
                if(this->readPathPoint(_blob, &offset, &command.p0) == false || this->readPathPoint(_blob, &offset, &command.p1) == false ||
                    this->readPathPoint(_blob, &offset, &command.p2) == false)
                {
                    return;
                }
                break;
            default:
                _path->commands.clear();
                return;
            }

            _path->commands.emplace_back(command);
        }

        _path->commandsDecoded = _path->commands.empty() == false;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::resolvePathCommands(CanvasPathDesc * const _path) const
    {
        if(_path->commandsBlob < m_blobBaseIndex)
        {
            return;
        }

        const std::uint32_t blobIndex = _path->commandsBlob - m_blobBaseIndex;
        if(blobIndex >= m_blobs.size())
        {
            return;
        }

        this->decodePathCommands(m_blobs[blobIndex], _path);
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::resolveGeometryBlobs()
    {
        for(CanvasNodeRecord & record : m_records)
        {
            for(CanvasPathDesc & path : record.node.fillGeometry)
            {
                this->resolvePathCommands(&path);
            }

            for(CanvasPathDesc & path : record.node.strokeGeometry)
            {
                this->resolvePathCommands(&path);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::resolvePaintStyleReferences()
    {
        using CanvasPaintStyleNodeMap = FigmaUnorderedMap<FigmaString, const CanvasNodeDesc *>;

        CanvasPaintStyleNodeMap styles(m_memory);
        for(const CanvasNodeRecord & record : m_records)
        {
            if(record.node.id.empty() == false && (record.node.fills.empty() == false || record.node.strokes.empty() == false))
            {
                styles.emplace(record.node.id, &record.node);
            }
        }

        for(CanvasNodeRecord & record : m_records)
        {
            if(record.node.fillStyleNodeId.empty() == false)
            {
                const auto it = styles.find(record.node.fillStyleNodeId);
                if(it != styles.end() && it->second->fills.empty() == false)
                {
                    record.node.fills = it->second->fills;
                }
            }

            if(record.node.strokeFillStyleNodeId.empty() == false)
            {
                const auto it = styles.find(record.node.strokeFillStyleNodeId);
                if(it != styles.end() && it->second->fills.empty() == false)
                {
                    record.node.strokes = it->second->fills;
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::resolvePathStylePaints()
    {
        using CanvasPaintStyleMap = FigmaUnorderedMap<std::uint32_t, const CanvasPaintVector *>;

        CanvasPaintStyleMap fillStyles(m_memory);
        CanvasPaintStyleMap strokeStyles(m_memory);

        for(const CanvasNodeRecord & record : m_records)
        {
            if(record.styleId == 0)
            {
                continue;
            }

            if(record.fillStyle == true && record.node.fills.empty() == false)
            {
                fillStyles.emplace(record.styleId, &record.node.fills);
            }

            if(record.strokeStyle == true && record.node.strokes.empty() == false)
            {
                strokeStyles.emplace(record.styleId, &record.node.strokes);
            }
        }

        for(CanvasNodeRecord & record : m_records)
        {
            for(CanvasPathDesc & path : record.node.fillGeometry)
            {
                if(path.styleId == 0)
                {
                    continue;
                }

                if(const CanvasPaintVector * paints = this->findPathStyleOverridePaints(record.node, path.styleId, true))
                {
                    path.paints = *paints;
                }
                else
                {
                    const auto it = fillStyles.find(path.styleId);
                    if(it != fillStyles.end())
                    {
                        path.paints = *it->second;
                    }
                }
            }

            for(CanvasPathDesc & path : record.node.strokeGeometry)
            {
                if(path.styleId == 0)
                {
                    continue;
                }

                if(const CanvasPaintVector * paints = this->findPathStyleOverridePaints(record.node, path.styleId, false))
                {
                    path.paints = *paints;
                }
                else
                {
                    const auto it = strokeStyles.find(path.styleId);
                    if(it != strokeStyles.end())
                    {
                        path.paints = *it->second;
                    }
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasPaintVector * CanvasDocumentDecoder::findPathStyleOverridePaints(const CanvasNodeDesc & _node, std::uint32_t _styleId, bool _fill) const
    {
        for(const CanvasPathStyleOverrideDesc & styleOverride : _node.pathStyleOverrides)
        {
            if(styleOverride.styleId != _styleId)
            {
                continue;
            }

            const CanvasPaintVector & paints = _fill == true ? styleOverride.fills : styleOverride.strokes;
            if(paints.empty() == false)
            {
                return &paints;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeFilterColorAdjust(KiwiByteReader & _reader, CanvasPaint * const _paint)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "FilterColorAdjust");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing FilterColorAdjust schema");
        }

        _paint->hasFilterColorAdjustValue = true;
        for(const KiwiFieldDesc & field : definition->fields)
        {
            float * target = nullptr;
            if(field.name == "tint")
            {
                target = &_paint->filterColorAdjust[0];
            }
            else if(field.name == "shadows")
            {
                target = &_paint->filterColorAdjust[1];
            }
            else if(field.name == "highlights")
            {
                target = &_paint->filterColorAdjust[2];
            }
            else if(field.name == "detail")
            {
                target = &_paint->filterColorAdjust[3];
            }
            else if(field.name == "exposure")
            {
                target = &_paint->filterColorAdjust[4];
            }
            else if(field.name == "vignette")
            {
                target = &_paint->filterColorAdjust[5];
            }
            else if(field.name == "temperature")
            {
                target = &_paint->filterColorAdjust[6];
            }
            else if(field.name == "vibrance")
            {
                target = &_paint->filterColorAdjust[7];
            }

            if(target != nullptr)
            {
                *target = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field.type, field.array);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodePaintFilter(KiwiByteReader & _reader, CanvasPaint * const _paint)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "PaintFilterMessage");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing PaintFilterMessage schema");
        }

        _paint->hasPaintFilterValue = true;
        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown PaintFilterMessage field");
            }

            float * target = nullptr;
            if(field->name == "tint")
            {
                target = &_paint->paintFilter[0];
            }
            else if(field->name == "shadows")
            {
                target = &_paint->paintFilter[1];
            }
            else if(field->name == "highlights")
            {
                target = &_paint->paintFilter[2];
            }
            else if(field->name == "detail")
            {
                target = &_paint->paintFilter[3];
            }
            else if(field->name == "exposure")
            {
                target = &_paint->paintFilter[4];
            }
            else if(field->name == "vignette")
            {
                target = &_paint->paintFilter[5];
            }
            else if(field->name == "temperature")
            {
                target = &_paint->paintFilter[6];
            }
            else if(field->name == "vibrance")
            {
                target = &_paint->paintFilter[7];
            }
            else if(field->name == "contrast")
            {
                target = &_paint->paintFilter[8];
            }
            else if(field->name == "brightness")
            {
                target = &_paint->paintFilter[9];
            }

            if(target != nullptr)
            {
                *target = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasArcDataDesc CanvasDocumentDecoder::decodeArcData(KiwiByteReader & _reader)
    {
        CanvasArcDataDesc value;

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "ArcData");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing ArcData schema");
        }

        value.valid = true;
        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown ArcData field");
            }

            if(field->name == "startingAngle")
            {
                value.startingAngle = _reader.readVarFloat();
            }
            else if(field->name == "endingAngle")
            {
                value.endingAngle = _reader.readVarFloat();
            }
            else if(field->name == "innerRadius")
            {
                value.innerRadius = _reader.readVarFloat();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    EPrototypeEventType CanvasDocumentDecoder::prototypeEventTypeFromString(FigmaStringView _value) const
    {
        if(_value == "ON_CLICK")
        {
            return EPrototypeEventType::Click;
        }

        if(_value == "ON_HOVER" || _value == "MOUSE_ENTER")
        {
            return EPrototypeEventType::HoverEnter;
        }

        if(_value == "MOUSE_LEAVE")
        {
            return EPrototypeEventType::HoverLeave;
        }

        if(_value == "ON_PRESS")
        {
            return EPrototypeEventType::Press;
        }

        if(_value == "MOUSE_DOWN")
        {
            return EPrototypeEventType::PointerDown;
        }

        if(_value == "MOUSE_UP")
        {
            return EPrototypeEventType::PointerUp;
        }

        if(_value == "AFTER_TIMEOUT")
        {
            return EPrototypeEventType::AfterTimeout;
        }

        if(_value == "ON_KEY_DOWN" || _value == "KEY_DOWN")
        {
            return EPrototypeEventType::KeyDown;
        }

        return EPrototypeEventType::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    EPrototypeConnectionType CanvasDocumentDecoder::prototypeConnectionTypeFromString(FigmaStringView _value) const
    {
        if(_value == "NONE")
        {
            return EPrototypeConnectionType::None;
        }

        if(_value == "INTERNAL_NODE")
        {
            return EPrototypeConnectionType::InternalNode;
        }

        if(_value == "BACK")
        {
            return EPrototypeConnectionType::Back;
        }

        if(_value == "CLOSE")
        {
            return EPrototypeConnectionType::Close;
        }

        return EPrototypeConnectionType::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    ECanvasBlendMode CanvasDocumentDecoder::blendModeFromString(FigmaStringView _value) const
    {
        if(_value == "PASS_THROUGH")
        {
            return ECanvasBlendMode::PassThrough;
        }

        if(_value == "NORMAL")
        {
            return ECanvasBlendMode::Normal;
        }

        if(_value == "MULTIPLY")
        {
            return ECanvasBlendMode::Multiply;
        }

        if(_value == "SCREEN")
        {
            return ECanvasBlendMode::Screen;
        }

        if(_value == "OVERLAY")
        {
            return ECanvasBlendMode::Overlay;
        }

        if(_value == "DARKEN")
        {
            return ECanvasBlendMode::Darken;
        }

        if(_value == "LIGHTEN")
        {
            return ECanvasBlendMode::Lighten;
        }

        if(_value == "COLOR_DODGE")
        {
            return ECanvasBlendMode::ColorDodge;
        }

        if(_value == "COLOR_BURN")
        {
            return ECanvasBlendMode::ColorBurn;
        }

        if(_value == "SOFT_LIGHT")
        {
            return ECanvasBlendMode::SoftLight;
        }

        if(_value == "HARD_LIGHT")
        {
            return ECanvasBlendMode::HardLight;
        }

        if(_value == "DIFFERENCE")
        {
            return ECanvasBlendMode::Difference;
        }

        if(_value == "EXCLUSION")
        {
            return ECanvasBlendMode::Exclusion;
        }

        if(_value == "HUE")
        {
            return ECanvasBlendMode::Hue;
        }

        if(_value == "SATURATION")
        {
            return ECanvasBlendMode::Saturation;
        }

        if(_value == "COLOR")
        {
            return ECanvasBlendMode::Color;
        }

        if(_value == "LUMINOSITY")
        {
            return ECanvasBlendMode::Luminosity;
        }

        return ECanvasBlendMode::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    EPrototypeNavigationType CanvasDocumentDecoder::prototypeNavigationTypeFromString(FigmaStringView _value) const
    {
        if(_value == "NAVIGATE")
        {
            return EPrototypeNavigationType::Navigate;
        }

        if(_value == "OVERLAY")
        {
            return EPrototypeNavigationType::Overlay;
        }

        if(_value == "SWAP" || _value == "SWAP_STATE")
        {
            return EPrototypeNavigationType::Swap;
        }

        if(_value == "SCROLL_TO")
        {
            return EPrototypeNavigationType::ScrollTo;
        }

        return EPrototypeNavigationType::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    EPrototypeTransitionType CanvasDocumentDecoder::prototypeTransitionTypeFromString(FigmaStringView _value) const
    {
        if(_value == "NONE" || _value == "INSTANT")
        {
            return EPrototypeTransitionType::Instant;
        }

        if(_value == "DISSOLVE")
        {
            return EPrototypeTransitionType::Dissolve;
        }

        if(_value == "SMART_ANIMATE")
        {
            return EPrototypeTransitionType::SmartAnimate;
        }

        if(_value == "MOVE_IN")
        {
            return EPrototypeTransitionType::MoveIn;
        }

        if(_value == "MOVE_OUT")
        {
            return EPrototypeTransitionType::MoveOut;
        }

        if(_value == "PUSH")
        {
            return EPrototypeTransitionType::Push;
        }

        if(_value == "SLIDE_IN")
        {
            return EPrototypeTransitionType::SlideIn;
        }

        if(_value == "SLIDE_OUT")
        {
            return EPrototypeTransitionType::SlideOut;
        }

        return EPrototypeTransitionType::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    EPrototypeTransitionDirection CanvasDocumentDecoder::prototypeTransitionDirectionFromString(FigmaStringView _value) const
    {
        if(_value.empty() == true || _value == "NONE")
        {
            return EPrototypeTransitionDirection::None;
        }

        if(_value == "LEFT")
        {
            return EPrototypeTransitionDirection::Left;
        }

        if(_value == "RIGHT")
        {
            return EPrototypeTransitionDirection::Right;
        }

        if(_value == "UP")
        {
            return EPrototypeTransitionDirection::Up;
        }

        if(_value == "DOWN")
        {
            return EPrototypeTransitionDirection::Down;
        }

        return EPrototypeTransitionDirection::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    EAnimationEasing CanvasDocumentDecoder::animationEasingFromString(FigmaStringView _value) const
    {
        if(_value == "LINEAR")
        {
            return EAnimationEasing::Linear;
        }

        if(_value == "EASE_IN")
        {
            return EAnimationEasing::EaseIn;
        }

        if(_value == "EASE_OUT")
        {
            return EAnimationEasing::EaseOut;
        }

        if(_value == "EASE_IN_AND_OUT" || _value == "EASE_IN_OUT")
        {
            return EAnimationEasing::EaseInOut;
        }

        if(_value == "IN_CUBIC")
        {
            return EAnimationEasing::InCubic;
        }

        if(_value == "OUT_CUBIC")
        {
            return EAnimationEasing::OutCubic;
        }

        if(_value == "IN_OUT_CUBIC")
        {
            return EAnimationEasing::InOutCubic;
        }

        return EAnimationEasing::Unsupported;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::appendUnsupportedField(UnsupportedFieldVector * const _fields, FigmaStringView _name) const
    {
        for(const FigmaString & field : *_fields)
        {
            if(field == _name)
            {
                return;
            }
        }

        _fields->emplace_back(_name.begin(), _name.end());
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodePrototypeEvent(KiwiByteReader & _reader, PrototypeInteractionDesc * const _interaction)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "PrototypeEvent");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing PrototypeEvent schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown PrototypeEvent field");
            }

            if(field->name == "interactionType")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                _interaction->rawEventType = value;
                _interaction->eventType = this->prototypeEventTypeFromString(value);
            }
            else if(field->name == "transitionTimeout")
            {
                _interaction->transitionTimeout = _reader.readVarFloat();
            }
            else if(field->name == "keyCode")
            {
                _interaction->keyCode = _reader.readVarUint();
            }
            else
            {
                if(field->name != "isDeleted")
                {
                    this->appendUnsupportedField(&_interaction->unsupportedFields, field->name);
                }
                this->skipValue(_reader, field->type, field->array);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeActionDesc CanvasDocumentDecoder::decodePrototypeAction(KiwiByteReader & _reader)
    {
        PrototypeActionDesc action(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "PrototypeAction");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing PrototypeAction schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown PrototypeAction field");
            }

            if(field->name == "transitionNodeID")
            {
                action.targetNodeId = this->decodeGuid(_reader);
            }
            else if(field->name == "connectionType")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                action.rawConnectionType = value;
                action.connectionType = this->prototypeConnectionTypeFromString(value);
            }
            else if(field->name == "navigationType")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                action.rawNavigationType = value;
                action.navigationType = this->prototypeNavigationTypeFromString(value);
            }
            else if(field->name == "transitionType" && this->isEnumType(field->type) == true)
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                action.rawTransitionType = value;
                action.transitionType = this->prototypeTransitionTypeFromString(value);
            }
            else if(field->name == "transitionDirection" && this->isEnumType(field->type) == true)
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                action.rawTransitionDirection = value;
                action.transitionDirection = this->prototypeTransitionDirectionFromString(value);
            }
            else if((field->name == "transitionEasing" || field->name == "easing" || field->name == "easingType") && this->isEnumType(field->type) == true)
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                action.rawTransitionEasing = value;
                action.transitionEasing = this->animationEasingFromString(value);
            }
            else if(field->name == "transitionDuration")
            {
                action.transitionDuration = _reader.readVarFloat();
            }
            else if(field->name == "transitionShouldSmartAnimate")
            {
                action.smartAnimate = _reader.readByte() != 0;
            }
            else if(field->name == "transitionPreserveScroll")
            {
                action.transitionPreserveScroll = _reader.readByte() != 0;
            }
            else if(field->name == "transitionResetVideoPosition")
            {
                action.transitionResetVideoPosition = _reader.readByte() != 0;
            }
            else if(field->name == "easingFunction")
            {
                action.hasEasingFunctionValue = true;
                this->skipValue(_reader, field->type, field->array);
            }
            else
            {
                if(field->name != "isDeleted")
                {
                    this->appendUnsupportedField(&action.unsupportedFields, field->name);
                }
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return action;
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeInteractionDesc CanvasDocumentDecoder::decodePrototypeInteraction(KiwiByteReader & _reader)
    {
        PrototypeInteractionDesc interaction(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "PrototypeInteraction");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing PrototypeInteraction schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown PrototypeInteraction field");
            }

            if(field->name == "id")
            {
                interaction.id = this->decodeGuid(_reader);
            }
            else if(field->name == "event")
            {
                this->decodePrototypeEvent(_reader, &interaction);
            }
            else if(field->name == "actions")
            {
                const std::uint32_t count = _reader.readVarUint();
                interaction.actions.reserve(count);
                for(std::uint32_t index = 0; index != count; ++index)
                {
                    interaction.actions.emplace_back(this->decodePrototypeAction(_reader));
                }
            }
            else
            {
                if(field->name != "isDeleted")
                {
                    this->appendUnsupportedField(&interaction.unsupportedFields, field->name);
                }
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return interaction;
    }
    //////////////////////////////////////////////////////////////////////////
    TextBaselineDesc CanvasDocumentDecoder::decodeBaseline(KiwiByteReader & _reader)
    {
        TextBaselineDesc value{};

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Baseline");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Baseline schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown Baseline field");
            }

            if(field->name == "position")
            {
                value.position = this->decodeVector(_reader);
            }
            else if(field->name == "width")
            {
                value.width = _reader.readVarFloat();
            }
            else if(field->name == "lineY")
            {
                value.lineY = _reader.readVarFloat();
            }
            else if(field->name == "lineHeight")
            {
                value.lineHeight = _reader.readVarFloat();
            }
            else if(field->name == "lineAscent")
            {
                value.lineAscent = _reader.readVarFloat();
            }
            else if(field->name == "firstCharacter")
            {
                value.firstCharacter = _reader.readVarUint();
            }
            else if(field->name == "endCharacter")
            {
                value.endCharacter = _reader.readVarUint();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeBaselineArray(KiwiByteReader & _reader, TextBaselineVector * const _baselines)
    {
        const std::uint32_t count = _reader.readVarUint();
        _baselines->reserve(_baselines->size() + count);
        for(std::uint32_t index = 0; index != count; ++index)
        {
            _baselines->emplace_back(this->decodeBaseline(_reader));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeTextData(KiwiByteReader & _reader, CanvasNodeDesc * const _node)
    {
        FigmaString characters(m_memory);
        TextBaselineVector baselines(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "TextData");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing TextData schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown TextData field");
            }

            if(field->name == "characters")
            {
                characters = _reader.readString(m_memory);
            }
            else if(field->name == "baselines")
            {
                this->decodeBaselineArray(_reader, &baselines);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        _node->text = characters;
        _node->textLines.clear();
        _node->textLines.reserve(baselines.size());
        for(const TextBaselineDesc & baseline : baselines)
        {
            if(baseline.endCharacter <= baseline.firstCharacter)
            {
                continue;
            }

            CanvasTextLineDesc line(m_memory);
            line.text = Detail::utf8Substring(m_memory, characters, baseline.firstCharacter, baseline.endCharacter);
            while(line.text.empty() == false && (line.text.back() == '\n' || line.text.back() == '\r'))
            {
                line.text.pop_back();
            }

            line.x = baseline.position.x;
            line.y = baseline.lineY != 0.0f ? baseline.lineY : baseline.position.y - baseline.lineAscent;
            line.width = baseline.width;
            line.lineHeight = baseline.lineHeight;
            line.lineAscent = baseline.lineAscent;
            _node->textLines.emplace_back(std::move(line));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::applyBaselinesToTextNode(const TextBaselineVector & _baselines, CanvasNodeDesc * const _node)
    {
        if(_node->text.empty() == true)
        {
            return;
        }

        _node->textLines.clear();
        _node->textLines.reserve(_baselines.size());
        for(const TextBaselineDesc & baseline : _baselines)
        {
            if(baseline.endCharacter <= baseline.firstCharacter)
            {
                continue;
            }

            CanvasTextLineDesc line(m_memory);
            line.text = Detail::utf8Substring(m_memory, _node->text, baseline.firstCharacter, baseline.endCharacter);
            while(line.text.empty() == false && (line.text.back() == '\n' || line.text.back() == '\r'))
            {
                line.text.pop_back();
            }

            line.x = baseline.position.x;
            line.y = baseline.lineY != 0.0f ? baseline.lineY : baseline.position.y - baseline.lineAscent;
            line.width = baseline.width;
            line.lineHeight = baseline.lineHeight;
            line.lineAscent = baseline.lineAscent;
            _node->textLines.emplace_back(std::move(line));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeDerivedTextData(KiwiByteReader & _reader, CanvasNodeDesc * const _node)
    {
        TextBaselineVector baselines(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "DerivedTextData");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing DerivedTextData schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown DerivedTextData field");
            }

            if(field->name == "baselines")
            {
                this->decodeBaselineArray(_reader, &baselines);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        this->applyBaselinesToTextNode(baselines, _node);
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasDocumentDecoder::decodeImageHash(KiwiByteReader & _reader)
    {
        FigmaString hash(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Image");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Image schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown Image field");
            }

            if(field->name == "hash")
            {
                FigmaByteBuffer bytes = _reader.readByteArray(m_memory);
                hash = Detail::bytesToHex(m_memory, bytes);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return hash;
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasDocumentDecoder::decodeSymbolData(KiwiByteReader & _reader)
    {
        FigmaString symbolId(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "SymbolData");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing SymbolData schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown SymbolData field");
            }

            if(field->name == "symbolID")
            {
                symbolId = this->decodeGuid(_reader);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return symbolId;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeVectorData(KiwiByteReader & _reader, CanvasNodeDesc * const _node)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "VectorData");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing VectorData schema");
        }

        _node->hasVectorDataValue = true;
        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown VectorData field");
            }

            if(field->name == "vectorNetworkBlob")
            {
                _node->vectorNetworkBlob = _reader.readVarUint();
                _node->hasVectorNetworkBlobValue = true;
            }
            else if(field->name == "normalizedSize")
            {
                _node->vectorNormalizedSize = this->decodeVector(_reader);
            }
            else if(field->name == "styleOverrideTable")
            {
                const std::uint32_t count = _reader.readVarUint();
                _node->pathStyleOverrides.reserve(_node->pathStyleOverrides.size() + count);
                for(std::uint32_t index = 0; index != count; ++index)
                {
                    CanvasNodeRecord overrideRecord(m_memory);
                    this->decodeNodeChange(_reader, &overrideRecord);

                    if(overrideRecord.styleId == 0)
                    {
                        continue;
                    }

                    CanvasPathStyleOverrideDesc styleOverride(m_memory);
                    styleOverride.styleId = overrideRecord.styleId;
                    styleOverride.fills = overrideRecord.node.fills;
                    styleOverride.strokes = overrideRecord.node.strokes;
                    _node->pathStyleOverrides.emplace_back(std::move(styleOverride));
                }
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasPaint CanvasDocumentDecoder::decodePaint(KiwiByteReader & _reader)
    {
        CanvasPaint paint(m_memory);

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "Paint");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing Paint schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown Paint field");
            }

            if(field->name == "type")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                paint.rawType = value;
                if(value == "IMAGE")
                {
                    paint.type = ECanvasPaintType::Image;
                }
                else if(value == "SOLID")
                {
                    paint.type = ECanvasPaintType::Solid;
                }
                else
                {
                    paint.type = ECanvasPaintType::Unsupported;
                }
            }
            else if(field->name == "color")
            {
                paint.color = this->decodeColor(_reader);
            }
            else if(field->name == "opacity")
            {
                paint.opacity = _reader.readVarFloat();
            }
            else if(field->name == "visible")
            {
                paint.visible = _reader.readByte() != 0;
            }
            else if(field->name == "blendMode")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                paint.rawBlendMode = value;
                paint.blendMode = this->blendModeFromString(value);
            }
            else if(field->name == "transform")
            {
                const MatrixDesc matrix = this->decodeMatrix(_reader);
                paint.transform[0] = matrix.m00;
                paint.transform[1] = matrix.m01;
                paint.transform[2] = matrix.m02;
                paint.transform[3] = matrix.m10;
                paint.transform[4] = matrix.m11;
                paint.transform[5] = matrix.m12;
                paint.hasTransformValue = true;
            }
            else if(field->name == "image")
            {
                paint.assetId = this->decodeImageHash(_reader);
            }
            else if(field->name == "imageScaleMode")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "STRETCH")
                {
                    paint.imageScaleMode = ECanvasImageScaleMode::Stretch;
                }
                else if(value == "FIT")
                {
                    paint.imageScaleMode = ECanvasImageScaleMode::Fit;
                }
                else if(value == "FILL")
                {
                    paint.imageScaleMode = ECanvasImageScaleMode::Fill;
                }
                else if(value == "TILE")
                {
                    paint.imageScaleMode = ECanvasImageScaleMode::Tile;
                }
                else
                {
                    paint.imageScaleMode = ECanvasImageScaleMode::Unknown;
                }
            }
            else if(field->name == "filterColorAdjust")
            {
                this->decodeFilterColorAdjust(_reader, &paint);
            }
            else if(field->name == "paintFilter")
            {
                this->decodePaintFilter(_reader, &paint);
            }
            else if(field->name == "originalImageWidth")
            {
                paint.originalImageWidth = _reader.readVarUint();
            }
            else if(field->name == "originalImageHeight")
            {
                paint.originalImageHeight = _reader.readVarUint();
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        return paint;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodePaintArray(KiwiByteReader & _reader, CanvasPaintVector * const _paints)
    {
        const std::uint32_t count = _reader.readVarUint();
        _paints->reserve(_paints->size() + count);
        for(std::uint32_t index = 0; index != count; ++index)
        {
            _paints->emplace_back(this->decodePaint(_reader));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    ECanvasNodeType CanvasDocumentDecoder::nodeTypeFromString(FigmaStringView _type) const
    {
        if(_type == "DOCUMENT")
        {
            return ECanvasNodeType::Document;
        }

        if(_type == "CANVAS")
        {
            return ECanvasNodeType::Canvas;
        }

        if(_type == "FRAME" || _type == "INSTANCE" || _type == "SYMBOL")
        {
            return ECanvasNodeType::Frame;
        }

        if(_type == "GROUP")
        {
            return ECanvasNodeType::Group;
        }

        if(_type == "RECTANGLE")
        {
            return ECanvasNodeType::Rectangle;
        }

        if(_type == "ROUNDED_RECTANGLE")
        {
            return ECanvasNodeType::RoundedRectangle;
        }

        if(_type == "ELLIPSE")
        {
            return ECanvasNodeType::Ellipse;
        }

        if(_type == "TEXT")
        {
            return ECanvasNodeType::Text;
        }

        if(_type == "VECTOR")
        {
            return ECanvasNodeType::Vector;
        }

        return ECanvasNodeType::Unknown;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::decodeNodeChange(KiwiByteReader & _reader, CanvasNodeRecord * const _record)
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, "NodeChange");
        if(definition == nullptr)
        {
            throw std::runtime_error("Missing NodeChange schema");
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                break;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown NodeChange field");
            }

            if(field->name == "guid")
            {
                _record->node.id = this->decodeGuid(_reader);
            }
            else if(field->name == "parentIndex")
            {
                ParentIndexDesc parent = this->decodeParentIndex(_reader);
                _record->parentId = parent.id;
                _record->position = parent.position;
            }
            else if(field->name == "type")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                _record->node.type = this->nodeTypeFromString(value);
            }
            else if(field->name == "name")
            {
                _record->node.name = _reader.readString(m_memory);
            }
            else if(field->name == "styleID")
            {
                _record->styleId = _reader.readVarUint();
            }
            else if(field->name == "isFillStyle")
            {
                _record->fillStyle = _reader.readByte() != 0;
            }
            else if(field->name == "isStrokeStyle")
            {
                _record->strokeStyle = _reader.readByte() != 0;
            }
            else if(field->name == "styleType")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                _record->fillStyle = _record->fillStyle || value == "FILL";
                _record->strokeStyle = _record->strokeStyle || value == "STROKE";
            }
            else if(field->name == "styleIdForFill")
            {
                _record->node.fillStyleNodeId = this->decodeStyleId(_reader);
            }
            else if(field->name == "styleIdForStrokeFill")
            {
                _record->node.strokeFillStyleNodeId = this->decodeStyleId(_reader);
            }
            else if(field->name == "visible")
            {
                _record->node.visible = _reader.readByte() != 0;
            }
            else if(field->name == "opacity")
            {
                _record->node.opacity = _reader.readVarFloat();
            }
            else if(field->name == "blendMode")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                _record->node.rawBlendMode = value;
                _record->node.blendMode = this->blendModeFromString(value);
            }
            else if(field->name == "size")
            {
                _record->size = this->decodeVector(_reader);
            }
            else if(field->name == "transform")
            {
                _record->transform = this->decodeMatrix(_reader);
            }
            else if(field->name == "cornerRadius")
            {
                _record->node.cornerRadius = _reader.readVarFloat();
            }
            else if(field->name == "strokeWeight")
            {
                _record->node.strokeWeight = _reader.readVarFloat();
            }
            else if(field->name == "strokeAlign")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "CENTER")
                {
                    _record->node.strokeAlign = ECanvasStrokeAlign::Center;
                }
                else if(value == "INSIDE")
                {
                    _record->node.strokeAlign = ECanvasStrokeAlign::Inside;
                }
                else if(value == "OUTSIDE")
                {
                    _record->node.strokeAlign = ECanvasStrokeAlign::Outside;
                }
                else
                {
                    _record->node.strokeAlign = ECanvasStrokeAlign::Unsupported;
                }
            }
            else if(field->name == "strokeCap")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "NONE")
                {
                    _record->node.strokeCap = ECanvasStrokeCap::None;
                }
                else if(value == "ROUND")
                {
                    _record->node.strokeCap = ECanvasStrokeCap::Round;
                }
                else if(value == "SQUARE")
                {
                    _record->node.strokeCap = ECanvasStrokeCap::Square;
                }
                else
                {
                    _record->node.strokeCap = ECanvasStrokeCap::Unsupported;
                }
            }
            else if(field->name == "strokeJoin")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "MITER")
                {
                    _record->node.strokeJoin = ECanvasStrokeJoin::Miter;
                }
                else if(value == "BEVEL")
                {
                    _record->node.strokeJoin = ECanvasStrokeJoin::Bevel;
                }
                else if(value == "ROUND")
                {
                    _record->node.strokeJoin = ECanvasStrokeJoin::Round;
                }
                else
                {
                    _record->node.strokeJoin = ECanvasStrokeJoin::Unsupported;
                }
            }
            else if(field->name == "dashPattern")
            {
                this->decodeFloatArray(_reader, &_record->node.dashPattern);
            }
            else if(field->name == "mask")
            {
                _record->node.mask = _reader.readByte() != 0;
            }
            else if(field->name == "maskType")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "ALPHA")
                {
                    _record->node.maskType = ECanvasMaskType::Alpha;
                }
                else if(value == "OUTLINE")
                {
                    _record->node.maskType = ECanvasMaskType::Outline;
                }
                else if(value == "LUMINANCE")
                {
                    _record->node.maskType = ECanvasMaskType::Luminance;
                }
                else
                {
                    _record->node.maskType = ECanvasMaskType::Unknown;
                }
            }
            else if(field->name == "frameMaskDisabled")
            {
                _record->node.frameMaskDisabled = _reader.readByte() != 0;
            }
            else if(field->name == "rectangleTopLeftCornerRadius")
            {
                _record->node.cornerRadius = std::max(_record->node.cornerRadius, _reader.readVarFloat());
            }
            else if(field->name == "fontSize")
            {
                _record->node.fontSize = _reader.readVarFloat();
            }
            else if(field->name == "lineHeight")
            {
                const NumberDesc lineHeight = this->decodeNumber(_reader);
                _record->node.lineHeight = lineHeight.percent == true ? 0.0f : lineHeight.value;
            }
            else if(field->name == "fontName")
            {
                const FontNameDesc fontName = this->decodeFontName(_reader);
                _record->node.fontFamily = fontName.family;
                _record->node.fontStyle = fontName.style;
                _record->node.fontPostscriptName = fontName.postscript;
            }
            else if(field->name == "textAlignHorizontal")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "CENTER")
                {
                    _record->node.textAlignHorizontal = ECanvasTextAlignHorizontal::Center;
                }
                else if(value == "RIGHT")
                {
                    _record->node.textAlignHorizontal = ECanvasTextAlignHorizontal::Right;
                }
            }
            else if(field->name == "textAlignVertical")
            {
                const FigmaString value = this->readEnum(_reader, field->type);
                if(value == "CENTER")
                {
                    _record->node.textAlignVertical = ECanvasTextAlignVertical::Center;
                }
                else if(value == "BOTTOM")
                {
                    _record->node.textAlignVertical = ECanvasTextAlignVertical::Bottom;
                }
            }
            else if(field->name == "textData")
            {
                this->decodeTextData(_reader, &_record->node);
            }
            else if(field->name == "derivedTextData")
            {
                this->decodeDerivedTextData(_reader, &_record->node);
            }
            else if(field->name == "fillPaints" || field->name == "backgroundPaints")
            {
                this->decodePaintArray(_reader, &_record->node.fills);
            }
            else if(field->name == "strokePaints")
            {
                this->decodePaintArray(_reader, &_record->node.strokes);
            }
            else if(field->name == "fillGeometry")
            {
                _record->node.hasFillGeometryValue = true;
                this->decodePathArray(_reader, &_record->node.fillGeometry);
            }
            else if(field->name == "strokeGeometry")
            {
                _record->node.hasStrokeGeometryValue = true;
                this->decodePathArray(_reader, &_record->node.strokeGeometry);
            }
            else if(field->name == "vectorData")
            {
                this->decodeVectorData(_reader, &_record->node);
            }
            else if(field->name == "arcData")
            {
                _record->node.arcData = this->decodeArcData(_reader);
            }
            else if(field->name == "pathTrim")
            {
                this->skipValue(_reader, field->type, field->array);
            }
            else if(field->name == "prototypeStartNodeID")
            {
                _record->node.prototypeStartNodeId = this->decodeGuid(_reader);
            }
            else if(field->name == "prototypeStartingPoint")
            {
                _record->node.hasPrototypeStartingPointValue = true;
                this->skipValue(_reader, field->type, field->array);
            }
            else if(field->name == "prototypeInteractions")
            {
                const std::uint32_t count = _reader.readVarUint();
                _record->node.prototypeInteractionCount += count;
                for(std::uint32_t index = 0; index != count; ++index)
                {
                    _record->node.prototypeInteractions.emplace_back(this->decodePrototypeInteraction(_reader));
                }
            }
            else if(field->name == "symbolData")
            {
                _record->symbolId = this->decodeSymbolData(_reader);
            }
            else
            {
                this->skipValue(_reader, field->type, field->array);
            }
        }

        _record->node.rect = {0.0f, 0.0f, _record->size.x, _record->size.y};
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasNodeDesc CanvasDocumentDecoder::copyNodeRecursive(const CanvasNodeRecordVector & _records, std::size_t _index, const MatrixDesc & _parentTransform, bool _rootFrame)
    {
        const CanvasNodeRecord & record = _records[_index];
        CanvasNodeDesc node(m_memory);
        node.id = record.node.id;
        node.name = record.node.name;
        node.type = record.node.type;
        node.size = record.size;
        node.opacity = record.node.opacity;
        node.cornerRadius = record.node.cornerRadius;
        node.strokeWeight = record.node.strokeWeight;
        node.fontSize = record.node.fontSize;
        node.lineHeight = record.node.lineHeight;
        node.fontWeight = record.node.fontWeight;
        node.visible = record.node.visible;
        node.mask = record.node.mask;
        node.frameMaskDisabled = record.node.frameMaskDisabled;
        node.hasFillGeometryValue = record.node.hasFillGeometryValue;
        node.hasStrokeGeometryValue = record.node.hasStrokeGeometryValue;
        node.hasVectorDataValue = record.node.hasVectorDataValue;
        node.hasVectorNetworkBlobValue = record.node.hasVectorNetworkBlobValue;
        node.hasPrototypeStartingPointValue = record.node.hasPrototypeStartingPointValue;
        node.vectorNetworkBlob = record.node.vectorNetworkBlob;
        node.prototypeInteractionCount = record.node.prototypeInteractionCount;
        node.vectorNormalizedSize = record.node.vectorNormalizedSize;
        node.maskType = record.node.maskType;
        node.blendMode = record.node.blendMode;
        node.strokeAlign = record.node.strokeAlign;
        node.strokeCap = record.node.strokeCap;
        node.strokeJoin = record.node.strokeJoin;
        node.arcData = record.node.arcData;
        node.textAlignHorizontal = record.node.textAlignHorizontal;
        node.textAlignVertical = record.node.textAlignVertical;
        node.text = record.node.text;
        node.fontFamily = record.node.fontFamily;
        node.fontStyle = record.node.fontStyle;
        node.fontPostscriptName = record.node.fontPostscriptName;
        node.prototypeStartNodeId = record.node.prototypeStartNodeId;
        node.symbolId = record.symbolId;
        node.fillStyleNodeId = record.node.fillStyleNodeId;
        node.strokeFillStyleNodeId = record.node.strokeFillStyleNodeId;
        node.rawBlendMode = record.node.rawBlendMode;
        node.dashPattern = record.node.dashPattern;
        node.pathStyleOverrides = record.node.pathStyleOverrides;
        node.fillGeometry = record.node.fillGeometry;
        node.strokeGeometry = record.node.strokeGeometry;
        node.prototypeInteractions = record.node.prototypeInteractions;
        node.textLines = record.node.textLines;
        node.fills = record.node.fills;
        node.strokes = record.node.strokes;

        const MatrixDesc transform = _rootFrame == true ? MatrixDesc{} : Detail::multiply(_parentTransform, record.transform);
        node.rect = _rootFrame == true ? Rectf{0.0f, 0.0f, record.size.x, record.size.y} : Detail::transformedRect(transform, record.size);
        Detail::assignTransformedQuad(_rootFrame == true ? MatrixDesc{} : transform, record.size, node.quad);

        const CanvasNodeRecord * childSource = &record;
        if(record.children.empty() == true && record.symbolId.empty() == false)
        {
            const auto symbolIt = m_nodeIndex.find(record.symbolId);
            if(symbolIt != m_nodeIndex.end())
            {
                childSource = &m_records[symbolIt->second];
            }
        }

        node.children.reserve(childSource->children.size());
        for(std::size_t childIndex : childSource->children)
        {
            node.children.emplace_back(this->copyNodeRecursive(_records, childIndex, transform, false));
        }

        return node;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CanvasDocumentDecoder::buildDocumentTree()
    {
        if(m_records.empty() == true)
        {
            return false;
        }

        m_nodeIndex.clear();
        const std::size_t recordSize = m_records.size();
        for(std::size_t index = 0; index != recordSize; ++index)
        {
            if(m_records[index].node.id.empty() == false)
            {
                m_nodeIndex.emplace(m_records[index].node.id, index);
            }
        }

        std::size_t rootIndex = std::numeric_limits<std::size_t>::max();
        std::size_t parentlessIndex = std::numeric_limits<std::size_t>::max();
        std::size_t rootCandidateCount = 0;
        std::size_t parentlessCount = 0;
        for(std::size_t index = 0; index != recordSize; ++index)
        {
            CanvasNodeRecord & record = m_records[index];
            if(record.parentId.empty() == true)
            {
                ++parentlessCount;
                parentlessIndex = index;

                if(record.node.type == ECanvasNodeType::Document)
                {
                    ++rootCandidateCount;
                    rootIndex = index;
                }
            }

            if(record.parentId.empty() == false)
            {
                const auto it = m_nodeIndex.find(record.parentId);
                if(it != m_nodeIndex.end())
                {
                    m_records[it->second].children.emplace_back(index);
                }
            }
        }

        if(rootCandidateCount == 0 && parentlessCount == 1)
        {
            rootIndex = parentlessIndex;
            rootCandidateCount = 1;
        }

        if(rootCandidateCount != 1 || rootIndex == std::numeric_limits<std::size_t>::max())
        {
            return false;
        }

        for(CanvasNodeRecord & record : m_records)
        {
            std::sort(record.children.begin(), record.children.end(), [this](std::size_t _left, std::size_t _right) {
                return m_records[_left].position > m_records[_right].position;
            });
        }

        m_canvasRoot = this->copyNodeRecursive(m_records, rootIndex, MatrixDesc{}, false);
        this->collectPrototypeStartFrame(m_canvasRoot);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasDocumentDecoder::collectPrototypeStartFrame(const CanvasNodeDesc & _node)
    {
        if(m_prototypeStartNodeId.empty() == true && _node.prototypeStartNodeId.empty() == false)
        {
            m_prototypeStartNodeId = _node.prototypeStartNodeId;
        }
        else if(m_prototypeStartNodeId.empty() == true && _node.hasPrototypeStartingPointValue == true)
        {
            m_prototypeStartNodeId = _node.id;
        }

        for(const CanvasNodeDesc & child : _node.children)
        {
            this->collectPrototypeStartFrame(child);
        }
    }
    //////////////////////////////////////////////////////////////////////////
}
