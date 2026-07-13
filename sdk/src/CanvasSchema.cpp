#include "CanvasSchema.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    const KiwiDefinitionDesc * findKiwiDefinition( const KiwiSchemaDesc & _schema, FigmaStringView _name )
    {
        const std::size_t definitionSize = _schema.definitions.size();
        for( std::size_t definitionIndex = 0; definitionIndex != definitionSize; ++definitionIndex )
        {
            const KiwiDefinitionDesc & definition = _schema.definitions[definitionIndex];
            if( definition.name == _name )
            {
                return &definition;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    const KiwiFieldDesc * findKiwiField( const KiwiDefinitionDesc & _definition, std::uint32_t _value )
    {
        const std::size_t fieldSize = _definition.fields.size();
        for( std::size_t fieldIndex = 0; fieldIndex != fieldSize; ++fieldIndex )
        {
            const KiwiFieldDesc & field = _definition.fields[fieldIndex];
            if( field.value == _value )
            {
                return &field;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
}
