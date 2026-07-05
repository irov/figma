#pragma once

#include "Figma/RuntimeInterface.h"

namespace Figma
{
    class RuntimeAllocatorMemoryResource
        : public FigmaMemoryResource
    {
    public:
        explicit RuntimeAllocatorMemoryResource(const AllocatorDesc & _allocator);

    protected:
        void * do_allocate(std::size_t _bytes, std::size_t _alignment) override;
        void do_deallocate(void * _ptr, std::size_t _bytes, std::size_t _alignment) override;
        bool do_is_equal(const FigmaMemoryResource & _other) const noexcept override;

    protected:
        AllocatorDesc m_allocator;
    };
}
