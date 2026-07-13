#pragma once

#include "Figma/RuntimeInterface.h"

#include "RuntimeAllocatorMemoryResource.h"

namespace Figma
{
    class Runtime final
        : public RuntimeInterface
    {
    public:
        explicit Runtime( const RuntimeDesc & _desc );
        ~Runtime();

        EResult loadDocumentFromFigData( const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document ) override;
        EResult createPlayer( DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player ) override;
        FigmaMemoryResource * getMemory() const override;
        const RuntimeDesc & getDesc() const override;

    public:
        void destroy() override;

    protected:
        RuntimeDesc m_desc;
        RuntimeAllocatorMemoryResource m_allocatorMemory;
        FigmaMemoryResource * m_memory;
    };
}
