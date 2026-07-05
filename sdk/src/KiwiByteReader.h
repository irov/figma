#pragma once

#include "Figma/Types.h"

#include <kiwi/kiwi.h>

namespace Figma
{
    class KiwiByteReader
    {
    public:
        KiwiByteReader(const std::uint8_t * _data, std::size_t _size);

        bool eof() const;
        std::uint8_t readByte();
        std::uint32_t readVarUint();
        std::int32_t readVarInt();
        std::uint64_t readVarUint64();
        std::int64_t readVarInt64();
        float readVarFloat();
        FigmaString readString(FigmaMemoryResource * _memory);
        FigmaByteBuffer readByteArray(FigmaMemoryResource * _memory);

    protected:
        std::uint32_t readUtf8Codepoint();
        void appendUtf8(std::uint32_t _codepoint, FigmaString * const _result);

    protected:
        kiwi_reader_t m_reader = {};
    };
}
