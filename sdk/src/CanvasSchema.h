#pragma once

#include "Figma/Types.h"

namespace Figma
{
    enum class EKiwiDefinitionKind
    {
        Enum,
        Struct,
        Message
    };

    struct KiwiFieldDesc
    {
        explicit KiwiFieldDesc(FigmaMemoryResource * _memory)
            : name(_memory)
            , type(_memory)
        {
        }

        FigmaString name;
        FigmaString type;
        bool array = false;
        std::uint32_t value = 0;
    };

    using KiwiFieldVector = FigmaVector<KiwiFieldDesc>;

    struct KiwiDefinitionDesc
    {
        explicit KiwiDefinitionDesc(FigmaMemoryResource * _memory)
            : name(_memory)
            , fields(_memory)
        {
        }

        FigmaString name;
        EKiwiDefinitionKind kind = EKiwiDefinitionKind::Struct;
        KiwiFieldVector fields;
    };

    using KiwiDefinitionVector = FigmaVector<KiwiDefinitionDesc>;

    struct KiwiSchemaDesc
    {
        explicit KiwiSchemaDesc(FigmaMemoryResource * _memory)
            : definitions(_memory)
        {
        }

        KiwiDefinitionVector definitions;
    };
    //////////////////////////////////////////////////////////////////////////
    const KiwiDefinitionDesc * findKiwiDefinition(const KiwiSchemaDesc & _schema, FigmaStringView _name);
    const KiwiFieldDesc * findKiwiField(const KiwiDefinitionDesc & _definition, std::uint32_t _value);
}
