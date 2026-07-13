#pragma once

#include "Figma/Types.h"

namespace Figma
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        void * allocateMemoryBlock( FigmaMemoryResource * _memory, std::size_t _size ) noexcept;
        void deallocateMemoryBlock( FigmaMemoryResource * _memory, void * _block ) noexcept;
        std::size_t getMemoryBlockSize( const void * _block ) noexcept;
        //////////////////////////////////////////////////////////////////////////
    }
    //////////////////////////////////////////////////////////////////////////
}
