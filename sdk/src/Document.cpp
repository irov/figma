#include "Document.h"

#include "DocumentLoader.h"

#include <new>
#include <utility>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        static const CanvasNodeDesc * findCanvasNodeRecursive( const CanvasNodeDesc & _node, FigmaStringView _id )
        {
            if( _node.id == _id )
            {
                return &_node;
            }

            for( const CanvasNodeDesc & child : _node.children )
            {
                const CanvasNodeDesc * found = findCanvasNodeRecursive( child, _id );
                if( found != nullptr )
                {
                    return found;
                }
            }

            return nullptr;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Document::Document( RuntimeInterface * const _runtime, FigmaMemoryResource * _memory )
        : m_runtime( _runtime )
        , m_memory( _memory )
        , m_path( _memory )
        , m_fileName( _memory )
        , m_renderCoordinates{}
        , m_thumbnailSize{}
        , m_canvasRoot( _memory )
        , m_prototypeStartNodeId( _memory )
        , m_assets( _memory )
        , m_canvasBytes( _memory )
        , m_bindings( _memory )
        , m_actions( _memory )
        , m_diagnostics( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Document::~Document()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void Document::destroy()
    {
        FigmaMemoryResource * memory = m_memory;

        this->~Document();
        memory->deallocate( this, sizeof( Document ), alignof( Document ) );
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Document::loadUX( FigmaStringView _data )
    {
        try
        {
            BindingVector bindings( m_memory );
            ActionVector actions( m_memory );

            const EResult result = loadDocumentUX( m_memory, &m_diagnostics, _data, &bindings, &actions );
            if( result != EResult::Ok )
            {
                return result;
            }

            m_bindings = std::move( bindings );
            m_actions = std::move( actions );

            return EResult::Ok;
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
    const FigmaString & Document::getPath() const
    {
        return m_path;
    }
    //////////////////////////////////////////////////////////////////////////
    const FigmaString & Document::getFileName() const
    {
        return m_fileName;
    }
    //////////////////////////////////////////////////////////////////////////
    const Rectf & Document::getRenderCoordinates() const
    {
        return m_renderCoordinates;
    }
    //////////////////////////////////////////////////////////////////////////
    const Vec2f & Document::getThumbnailSize() const
    {
        return m_thumbnailSize;
    }
    //////////////////////////////////////////////////////////////////////////
    const AssetVector & Document::getAssets() const
    {
        return m_assets;
    }
    //////////////////////////////////////////////////////////////////////////
    const AssetDesc * Document::findAsset( FigmaStringView _assetId ) const
    {
        for( const AssetDesc & asset : m_assets )
        {
            if( asset.id == _assetId || asset.path == _assetId )
            {
                return &asset;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    const AssetDesc * Document::getThumbnailAsset() const
    {
        return this->findAsset( "thumbnail.png" );
    }
    //////////////////////////////////////////////////////////////////////////
    bool Document::getFrameRect(FigmaStringView _nodeId, Rectf * const _rect) const
    {
        if(_rect == nullptr)
        {
            return false;
        }

        const CanvasNodeDesc * node = this->findCanvasNodeDesc(_nodeId);
        if(node == nullptr)
        {
            return false;
        }

        *_rect = node->rect;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Document::getPrototypeStartFrameRect(Rectf * const _rect) const
    {
        if(_rect == nullptr)
        {
            return false;
        }

        const CanvasNodeDesc * node = this->getPrototypeStartFrameDesc();
        if(node == nullptr)
        {
            return false;
        }

        *_rect = node->rect;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    const DocumentInspectionInterface * Document::getInspection() const
    {
        return this;
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::getCanvasRoot() const
    {
        return this->getCanvasRootDesc();
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::findCanvasNode( FigmaStringView _nodeId ) const
    {
        return this->findCanvasNodeDesc( _nodeId );
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::getPrototypeStartFrame() const
    {
        return this->getPrototypeStartFrameDesc();
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::getCanvasRootDesc() const
    {
        return m_hasCanvasRoot == true ? &m_canvasRoot : nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::findCanvasNodeDesc( FigmaStringView _nodeId ) const
    {
        if( m_hasCanvasRoot == false || _nodeId.empty() == true )
        {
            return nullptr;
        }

        return Detail::findCanvasNodeRecursive( m_canvasRoot, _nodeId );
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::getPrototypeStartFrameDesc() const
    {
        if( m_hasCanvasRoot == false || m_prototypeStartNodeId.empty() == true )
        {
            return nullptr;
        }

        return this->findCanvasNodeDesc( m_prototypeStartNodeId );
    }
    //////////////////////////////////////////////////////////////////////////
    const BindingVector & Document::getBindings() const
    {
        return m_bindings;
    }
    //////////////////////////////////////////////////////////////////////////
    const ActionVector & Document::getActions() const
    {
        return m_actions;
    }
    //////////////////////////////////////////////////////////////////////////
    const DiagnosticsInterface * Document::getDiagnostics() const
    {
        return &m_diagnostics;
    }
    //////////////////////////////////////////////////////////////////////////
    DiagnosticsInterface * Document::getDiagnostics()
    {
        return &m_diagnostics;
    }
    //////////////////////////////////////////////////////////////////////////
    char Document::getCanvasVersion() const
    {
        return m_canvasVersion;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Document::hasCanvasBytes() const
    {
        return m_canvasBytes.empty() == false;
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaMemoryResource * Document::getMemory() const
    {
        return m_memory;
    }
    //////////////////////////////////////////////////////////////////////////
}
