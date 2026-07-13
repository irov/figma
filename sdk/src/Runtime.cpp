#include "Runtime.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Player.h"

#include <new>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        static bool isAllocatorValid( const AllocatorDesc & _allocator )
        {
            const bool hasAlloc = _allocator.alloc != nullptr;
            const bool hasFree = _allocator.free != nullptr;

            if( hasAlloc != hasFree )
            {
                return false;
            }

            if( _allocator.realloc != nullptr && hasAlloc == false )
            {
                return false;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static void deallocateRuntime( const AllocatorDesc & _allocator, void * _memory )
        {
            if( _memory == nullptr )
            {
                return;
            }

            if( _allocator.free != nullptr )
            {
                _allocator.free( _memory, _allocator.userData );
                return;
            }

            ::operator delete( _memory );
        }
        //////////////////////////////////////////////////////////////////////////
    }
    //////////////////////////////////////////////////////////////////////////
    Runtime::Runtime( const RuntimeDesc & _desc )
        : m_desc( _desc )
        , m_allocatorMemory( _desc.allocator )
        , m_memory( &m_allocatorMemory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Runtime::~Runtime()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Runtime::loadDocumentFromFigData( const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document )
    {
        try
        {
            return loadDocumentFromArchiveData( this, _data, _size, _options, _document );
        }
        catch( const std::bad_alloc & )
        {
            return EResult::OutOfMemory;
        }
        catch( ... )
        {
            return EResult::InvalidState;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Runtime::createPlayer( DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player )
    {
        try
        {
            return createPlayerFromDocument( _document, _desc, _player );
        }
        catch( const std::bad_alloc & )
        {
            return EResult::OutOfMemory;
        }
        catch( ... )
        {
            return EResult::InvalidState;
        }
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
        const AllocatorDesc allocator = m_desc.allocator;

        this->~Runtime();
        Detail::deallocateRuntime( allocator, this );
    }
    //////////////////////////////////////////////////////////////////////////
    EResult createRuntime( std::uint32_t _version, const RuntimeDesc & _desc, RuntimeInterface ** const _runtime )
    {
        if( _runtime == nullptr )
        {
            return EResult::InvalidArgument;
        }

        *_runtime = nullptr;

        if( _version != FIGMA_SDK_VERSION )
        {
            return EResult::VersionMismatch;
        }

        if( Detail::isAllocatorValid( _desc.allocator ) == false )
        {
            return EResult::InvalidArgument;
        }

        void * runtimeMemory = nullptr;

        try
        {
            if( _desc.allocator.alloc != nullptr )
            {
                runtimeMemory = _desc.allocator.alloc( sizeof( Runtime ), _desc.allocator.userData );
                if( runtimeMemory == nullptr )
                {
                    return EResult::OutOfMemory;
                }
            }
            else
            {
                runtimeMemory = ::operator new( sizeof( Runtime ) );
            }

            *_runtime = new( runtimeMemory ) Runtime( _desc );
        }
        catch( const std::bad_alloc & )
        {
            Detail::deallocateRuntime( _desc.allocator, runtimeMemory );

            return EResult::OutOfMemory;
        }
        catch( ... )
        {
            Detail::deallocateRuntime( _desc.allocator, runtimeMemory );

            return EResult::InvalidState;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}
