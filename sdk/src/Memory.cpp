#include "Memory.h"

#include <limits>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        struct alignas( std::max_align_t ) MemoryBlockHeader
        {
            std::size_t size;
        };
        //////////////////////////////////////////////////////////////////////////
        void * allocateMemoryBlock( FigmaMemoryResource * _memory, std::size_t _size ) noexcept
        {
            if( _memory == nullptr )
            {
                return nullptr;
            }

            if( _size > std::numeric_limits<std::size_t>::max() - sizeof( MemoryBlockHeader ) )
            {
                return nullptr;
            }

            const std::size_t allocationSize = sizeof( MemoryBlockHeader ) + _size;

            try
            {
                void * memory = _memory->allocate( allocationSize, alignof( MemoryBlockHeader ) );
                MemoryBlockHeader * header = static_cast<MemoryBlockHeader *>( memory );
                header->size = _size;

                return static_cast<void *>( header + 1 );
            }
            catch( ... )
            {
                return nullptr;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void deallocateMemoryBlock( FigmaMemoryResource * _memory, void * _block ) noexcept
        {
            if( _memory == nullptr || _block == nullptr )
            {
                return;
            }

            MemoryBlockHeader * header = static_cast<MemoryBlockHeader *>( _block ) - 1;
            const std::size_t allocationSize = sizeof( MemoryBlockHeader ) + header->size;
            _memory->deallocate( header, allocationSize, alignof( MemoryBlockHeader ) );
        }
        //////////////////////////////////////////////////////////////////////////
        std::size_t getMemoryBlockSize( const void * _block ) noexcept
        {
            if( _block == nullptr )
            {
                return 0;
            }

            const MemoryBlockHeader * header = static_cast<const MemoryBlockHeader *>( _block ) - 1;

            return header->size;
        }
        //////////////////////////////////////////////////////////////////////////
    }
    //////////////////////////////////////////////////////////////////////////
}
