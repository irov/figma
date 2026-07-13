#pragma once

#include "Figma/Types.h"

namespace Figma
{
    enum class EBindingValueType
    {
        None,
        Text,
        Number,
        Boolean,
        Image
    };

    struct BindingValue
    {
        explicit BindingValue( FigmaMemoryResource * _memory = getDefaultMemoryResource() )
            : stringValue( _memory )
        {
        }

        EBindingValueType type = EBindingValueType::None;
        FigmaString stringValue;
        double numberValue = 0.0;
        bool boolValue = false;
    };

    class DataContextInterface
    {
    public:
        virtual bool getBindingValue( FigmaStringView _key, BindingValue * const _value ) = 0;
        virtual bool isBindingDirty( FigmaStringView _key ) const = 0;

    protected:
        ~DataContextInterface() = default;
    };
}
