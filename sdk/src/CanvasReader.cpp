#include "CanvasReader.h"

#include <stdexcept>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    CanvasReader::CanvasReader(FigmaMemoryResource * _memory, const KiwiSchemaDesc & _schema)
        : m_memory(_memory)
        , m_schema(_schema)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void CanvasReader::skipValue(KiwiByteReader & _reader, FigmaStringView _type, bool _array) const
    {
        if(_array == true)
        {
            if(_type == "byte")
            {
                (void)_reader.readByteArray(m_memory);
                return;
            }

            const std::uint32_t count = _reader.readVarUint();
            for(std::uint32_t index = 0; index != count; ++index)
            {
                this->skipValue(_reader, _type, false);
            }

            return;
        }

        if(_type == "bool" || _type == "byte")
        {
            (void)_reader.readByte();
            return;
        }

        if(_type == "int")
        {
            (void)_reader.readVarInt();
            return;
        }

        if(_type == "uint")
        {
            (void)_reader.readVarUint();
            return;
        }

        if(_type == "float")
        {
            (void)_reader.readVarFloat();
            return;
        }

        if(_type == "string")
        {
            (void)_reader.readString(m_memory);
            return;
        }

        if(_type == "int64")
        {
            (void)_reader.readVarInt64();
            return;
        }

        if(_type == "uint64")
        {
            (void)_reader.readVarUint64();
            return;
        }

        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, _type);
        if(definition == nullptr)
        {
            throw std::runtime_error("Unknown Kiwi type");
        }

        if(definition->kind == EKiwiDefinitionKind::Enum)
        {
            (void)_reader.readVarUint();
            return;
        }

        if(definition->kind == EKiwiDefinitionKind::Struct)
        {
            for(const KiwiFieldDesc & field : definition->fields)
            {
                this->skipValue(_reader, field.type, field.array);
            }

            return;
        }

        for(;;)
        {
            const std::uint32_t tag = _reader.readVarUint();
            if(tag == 0)
            {
                return;
            }

            const KiwiFieldDesc * field = findKiwiField(*definition, tag);
            if(field == nullptr)
            {
                throw std::runtime_error("Unknown Kiwi message field");
            }

            this->skipValue(_reader, field->type, field->array);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString CanvasReader::readEnum(KiwiByteReader & _reader, FigmaStringView _type) const
    {
        const std::uint32_t value = _reader.readVarUint();
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, _type);
        if(definition == nullptr || definition->kind != EKiwiDefinitionKind::Enum)
        {
            return FigmaString(m_memory);
        }

        for(const KiwiFieldDesc & field : definition->fields)
        {
            if(field.value == value)
            {
                return field.name;
            }
        }

        return FigmaString(m_memory);
    }
    //////////////////////////////////////////////////////////////////////////
    bool CanvasReader::isEnumType(FigmaStringView _type) const
    {
        const KiwiDefinitionDesc * definition = findKiwiDefinition(m_schema, _type);
        return definition != nullptr && definition->kind == EKiwiDefinitionKind::Enum;
    }
    //////////////////////////////////////////////////////////////////////////
}
