#include "RuntimeAllocatorMemoryResource.h"

#include <cstdlib>
#include <new>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    RuntimeAllocatorMemoryResource::RuntimeAllocatorMemoryResource( const AllocatorDesc & _allocator )
        : m_allocator( _allocator )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    RuntimeAllocatorMemoryResource::~RuntimeAllocatorMemoryResource()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void * RuntimeAllocatorMemoryResource::do_allocate( std::size_t _bytes, std::size_t _alignment )
    {
        const std::size_t allocationSize = _bytes != 0 ? _bytes : 1;

        if( m_allocator.alloc != nullptr )
        {
            if( _alignment > alignof( std::max_align_t ) )
            {
                throw std::bad_alloc();
            }

            void * memory = m_allocator.alloc( allocationSize, m_allocator.userData );
            if( memory == nullptr )
            {
                throw std::bad_alloc();
            }

            return memory;
        }

        if( _alignment > alignof( std::max_align_t ) )
        {
            return ::operator new( allocationSize, std::align_val_t( _alignment ) );
        }

        void * memory = std::malloc( allocationSize );
        if( memory == nullptr )
        {
            throw std::bad_alloc();
        }

        return memory;
    }
    //////////////////////////////////////////////////////////////////////////
    void RuntimeAllocatorMemoryResource::do_deallocate( void * _ptr, std::size_t _bytes, std::size_t _alignment )
    {
        (void)_bytes;
        if( m_allocator.free != nullptr )
        {
            m_allocator.free( _ptr, m_allocator.userData );
            return;
        }

        if( _alignment > alignof( std::max_align_t ) )
        {
            ::operator delete( _ptr, std::align_val_t( _alignment ) );
            return;
        }

        std::free( _ptr );
    }
    //////////////////////////////////////////////////////////////////////////
    bool RuntimeAllocatorMemoryResource::do_is_equal( const FigmaMemoryResource & _other ) const noexcept
    {
        return this == &_other;
    }
    //////////////////////////////////////////////////////////////////////////
}
