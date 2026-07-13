#include "Document.h"

#include <cstdint>
#include <memory>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        static const CanvasNodeDesc * findCanvasNodeRecursive(const CanvasNodeDesc & _node, FigmaStringView _id)
        {
            if(_node.id == _id)
            {
                return &_node;
            }

            for(const CanvasNodeDesc & child : _node.children)
            {
                const CanvasNodeDesc * found = findCanvasNodeRecursive(child, _id);
                if(found != nullptr)
                {
                    return found;
                }
            }

            return nullptr;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasPathDesc::CanvasPathDesc(FigmaMemoryResource * _memory)
        : commands(_memory)
        , paints(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasPathStyleOverrideDesc::CanvasPathStyleOverrideDesc(FigmaMemoryResource * _memory)
        : fills(_memory)
        , strokes(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasTextLineDesc::CanvasTextLineDesc(FigmaMemoryResource * _memory)
        : text(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeActionDesc::PrototypeActionDesc(FigmaMemoryResource * _memory)
        : targetNodeId(_memory)
        , rawConnectionType(_memory)
        , rawNavigationType(_memory)
        , rawTransitionType(_memory)
        , rawTransitionDirection(_memory)
        , rawTransitionEasing(_memory)
        , unsupportedFields(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeInteractionDesc::PrototypeInteractionDesc(FigmaMemoryResource * _memory)
        : id(_memory)
        , rawEventType(_memory)
        , actions(_memory)
        , unsupportedFields(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    AnimationTrackDesc::AnimationTrackDesc(FigmaMemoryResource * _memory)
        : nodeId(_memory)
        , targetNodeId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    AnimationClipDesc::AnimationClipDesc(FigmaMemoryResource * _memory)
        : id(_memory)
        , sourceFrameId(_memory)
        , targetFrameId(_memory)
        , sourceNodeId(_memory)
        , tracks(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PlayerAnimationStateDesc::PlayerAnimationStateDesc(FigmaMemoryResource * _memory)
        : clip(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasNodeDesc::CanvasNodeDesc(FigmaMemoryResource * _memory)
        : id(_memory)
        , name(_memory)
        , rect{}
        , quad{}
        , vectorNormalizedSize{}
        , text(_memory)
        , fontFamily(_memory)
        , fontStyle(_memory)
        , fontPostscriptName(_memory)
        , prototypeStartNodeId(_memory)
        , symbolId(_memory)
        , fillStyleNodeId(_memory)
        , strokeFillStyleNodeId(_memory)
        , rawBlendMode(_memory)
        , dashPattern(_memory)
        , pathStyleOverrides(_memory)
        , fillGeometry(_memory)
        , strokeGeometry(_memory)
        , prototypeInteractions(_memory)
        , textLines(_memory)
        , fills(_memory)
        , strokes(_memory)
        , children(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    BindingDesc::BindingDesc(FigmaMemoryResource * _memory)
        : nodeId(_memory)
        , key(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    ActionDesc::ActionDesc(FigmaMemoryResource * _memory)
        : nodeId(_memory)
        , actionId(_memory)
        , targetFrameId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Document::Document(RuntimeInterface * const _runtime, FigmaMemoryResource * _memory)
        : m_runtime(_runtime)
        , m_memory(_memory)
        , m_path(_memory)
        , m_fileName(_memory)
        , m_renderCoordinates{}
        , m_thumbnailSize{}
        , m_canvasRoot(_memory)
        , m_prototypeStartNodeId(_memory)
        , m_assets(_memory)
        , m_canvasBytes(_memory)
        , m_bindings(_memory)
        , m_actions(_memory)
        , m_diagnostics(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void Document::destroy()
    {
        delete this;
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
    const AssetDesc * Document::findAsset(FigmaStringView _assetId) const
    {
        for(const AssetDesc & asset : m_assets)
        {
            if(asset.id == _assetId || asset.path == _assetId)
            {
                return &asset;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    const AssetDesc * Document::getThumbnailAsset() const
    {
        return this->findAsset("thumbnail.png");
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
    const CanvasNodeDesc * Document::findCanvasNode(FigmaStringView _nodeId) const
    {
        return this->findCanvasNodeDesc(_nodeId);
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
    const CanvasNodeDesc * Document::findCanvasNodeDesc(FigmaStringView _nodeId) const
    {
        if(m_hasCanvasRoot == false || _nodeId.empty() == true)
        {
            return nullptr;
        }

        return Detail::findCanvasNodeRecursive(m_canvasRoot, _nodeId);
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Document::getPrototypeStartFrameDesc() const
    {
        if(m_hasCanvasRoot == false || m_prototypeStartNodeId.empty() == true)
        {
            return nullptr;
        }

        return this->findCanvasNodeDesc(m_prototypeStartNodeId);
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
