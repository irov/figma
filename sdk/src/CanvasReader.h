#pragma once

#include "CanvasSchema.h"
#include "KiwiByteReader.h"

namespace Figma
{
    class CanvasReader
    {
    public:
        CanvasReader( FigmaMemoryResource * _memory, const KiwiSchemaDesc & _schema );
        ~CanvasReader();

        void skipValue( KiwiByteReader & _reader, FigmaStringView _type, bool _array ) const;
        FigmaString readEnum( KiwiByteReader & _reader, FigmaStringView _type ) const;
        bool isEnumType( FigmaStringView _type ) const;

    protected:
        FigmaMemoryResource * m_memory;
        const KiwiSchemaDesc & m_schema;
    };
}
