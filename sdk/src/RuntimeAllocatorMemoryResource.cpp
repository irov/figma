#include "RuntimeAllocatorMemoryResource.h"

#include <cstdlib>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    RuntimeAllocatorMemoryResource::RuntimeAllocatorMemoryResource(const AllocatorDesc & _allocator)
        : m_allocator(_allocator)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void * RuntimeAllocatorMemoryResource::do_allocate(std::size_t _bytes, std::size_t _alignment)
    {
        (void)_alignment;

        if(m_allocator.alloc != nullptr)
        {
            return m_allocator.alloc(_bytes, m_allocator.userData);
        }

        return std::malloc(_bytes);
    }
    //////////////////////////////////////////////////////////////////////////
    void RuntimeAllocatorMemoryResource::do_deallocate(void * _ptr, std::size_t _bytes, std::size_t _alignment)
    {
        (void)_bytes;
        (void)_alignment;

        if(m_allocator.free != nullptr)
        {
            m_allocator.free(_ptr, m_allocator.userData);
            return;
        }

        std::free(_ptr);
    }
    //////////////////////////////////////////////////////////////////////////
    bool RuntimeAllocatorMemoryResource::do_is_equal(const FigmaMemoryResource & _other) const noexcept
    {
        return this == &_other;
    }
    //////////////////////////////////////////////////////////////////////////
}
