#include "Runtime.h"

#include "DocumentLoader.h"
#include "PlayerFactory.h"

#include <new>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    Runtime::Runtime(const RuntimeDesc & _desc)
        : m_desc(_desc)
        //////////////////////////////////////////////////////////////////////////
        , m_allocatorMemory(std::make_unique<RuntimeAllocatorMemoryResource>(_desc.allocator))
        , m_memory(m_allocatorMemory.get())
    {
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Runtime::loadDocumentFromFigData(const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document)
    {
        return loadDocumentFromArchiveDataImpl(this, _data, _size, _options, _document);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Runtime::createPlayer(DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player)
    {
        return createPlayerImpl(_document, _desc, _player);
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaMemoryResource * Runtime::getMemory() const
    {
        return m_memory;
    }
    //////////////////////////////////////////////////////////////////////////
    const RuntimeDesc & Runtime::getDesc() const
    {
        return m_desc;
    }
    //////////////////////////////////////////////////////////////////////////
    void Runtime::destroy()
    {
        delete this;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult createRuntime(std::uint32_t _version, const RuntimeDesc & _desc, RuntimeInterface ** const _runtime)
    {
        if(_runtime == nullptr)
        {
            return EResult::InvalidArgument;
        }

        *_runtime = nullptr;

        if(_version != FIGMA_SDK_VERSION)
        {
            return EResult::VersionMismatch;
        }

        try
        {
            *_runtime = new Runtime(_desc);
        }
        catch(const std::bad_alloc &)
        {
            return EResult::OutOfMemory;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}
