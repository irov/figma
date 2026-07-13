#pragma once

#include "Figma/DocumentInterface.h"
#include "Figma/PlayerInterface.h"
#include "Figma/Types.h"

namespace Figma
{
    struct AllocatorDesc
    {
        // alloc/free must be supplied together; returned memory must satisfy max_align_t alignment.
        using AllocFunc = void * (*)(std::size_t _size, void * _userData);
        using ReallocFunc = void * (*)(void * _ptr, std::size_t _size, void * _userData);
        using FreeFunc = void (*)(void * _ptr, void * _userData);

        AllocFunc alloc = nullptr;
        ReallocFunc realloc = nullptr;
        FreeFunc free = nullptr;
        void * userData = nullptr;
    };

    struct RuntimeDesc
    {
        AllocatorDesc allocator;
    };

    class RuntimeInterface
    {
    public:
        virtual EResult loadDocumentFromFigData(const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document) = 0;
        virtual EResult createPlayer(DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player) = 0;
        virtual FigmaMemoryResource * getMemory() const = 0;
        virtual const RuntimeDesc & getDesc() const = 0;

    public:
        virtual void destroy() = 0;

    protected:
        ~RuntimeInterface() = default;
    };

    FIGMA_EXPORT EResult createRuntime(std::uint32_t _version, const RuntimeDesc & _desc, RuntimeInterface ** const _runtime);
}
