#pragma once

#include "Figma/PlayerInterface.h"

#include "Diagnostics.h"
#include "Document.h"
#include "RenderList.h"

namespace Figma
{
    class Player final
        : public PlayerInterface
    {
    public:
        Player(Document & _document, const PlayerDesc & _desc, FigmaMemoryResource * _memory);

        void destroy() override;
        EResult setActionRouter(ActionRouterInterface * _router) override;
        EResult setDataContext(DataContextInterface * _context) override;
        EResult inputPointer(const PointerEvent & _event) override;
        EResult inputKey(const KeyEvent & _event) override;
        EResult update(float _dt) override;
        EResult restart() override;

        EResult setText(FigmaStringView _key, FigmaStringView _value) override;
        EResult setNumber(FigmaStringView _key, double _value) override;
        EResult setVisible(FigmaStringView _key, bool _value) override;
        EResult setEnabled(FigmaStringView _key, bool _value) override;
        EResult setImage(FigmaStringView _key, FigmaStringView _assetId) override;
        EResult setState(FigmaStringView _key, bool _value) override;
        EResult setBindingValue(FigmaStringView _key, const BindingValue & _value) override;
        EResult clearBindingValue(FigmaStringView _key) override;

        const RenderListInterface * getRenderList() const override;
        const DiagnosticsInterface * getDiagnostics() const override;

    protected:
        struct Hotspot
        {
            explicit Hotspot(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            Rectf rect{};
            FigmaString nodeId;
            FigmaString actionId;
            FigmaString targetFrameId;
            const PrototypeActionDesc * prototypeAction = nullptr;
            EPrototypeEventType eventType = EPrototypeEventType::Click;
        };

        using HotspotVector = FigmaVector<Hotspot>;

        struct AnimatedNodeDesc
        {
            Rectf rect{};
            float opacity = 1.0f;
            bool hasRect = false;
            bool hasOpacity = false;
            bool matched = false;
        };

        using AnimatedNodeMap = FigmaUnorderedMap<FigmaString, AnimatedNodeDesc>;
        using NodeIdSet = FigmaUnorderedSet<FigmaString>;

        struct AnimationRenderContext
        {
            explicit AnimationRenderContext(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            AnimatedNodeMap targetNodes;
            NodeIdSet matchedNodeIds;
            NodeIdSet skipNodeIds;
            NodeIdSet opaqueNodeIds;
            FigmaString rootNodeId;
            float progress = 0.0f;
            bool targetPass = false;
            bool smartAnimate = false;
            bool skipRootGeometry = false;
            std::uint32_t renderLayerId = 0;
            float renderLayerOpacity = 1.0f;
        };

        struct NodeSwapState
        {
            explicit NodeSwapState(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            FigmaString currentNodeId;
            float startedAt = 0.0f;
        };

        struct LocalAnimationState
        {
            explicit LocalAnimationState(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            FigmaString sourceNodeId;
            FigmaString fromNodeId;
            FigmaString targetNodeId;
            EPrototypeTransitionType transitionType = EPrototypeTransitionType::Instant;
            EAnimationEasing easing = EAnimationEasing::EaseInOut;
            float elapsed = 0.0f;
            float duration = 0.0f;
            float progress = 0.0f;
            bool active = false;
        };

        using NodeSwapStateMap = FigmaUnorderedMap<FigmaString, NodeSwapState>;
        using LocalAnimationStateVector = FigmaVector<LocalAnimationState>;
        using BindingOverrideMap = FigmaUnorderedMap<FigmaString, BindingValue>;

        EResult routePointerAction(const Hotspot & _hotspot, const PointerEvent & _event);
        EResult navigateToFrame(FigmaStringView _targetFrameId, const PrototypeActionDesc * _action, FigmaStringView _sourceNodeId, float _initialElapsed = 0.0f);
        EResult swapNodeState(FigmaStringView _sourceNodeId, FigmaStringView _fromNodeId, const PrototypeActionDesc & _action, float _initialElapsed = 0.0f);
        EResult beginPrototypeAnimation(const CanvasNodeDesc & _sourceFrame, const CanvasNodeDesc & _targetFrame, const PrototypeActionDesc & _action, FigmaStringView _sourceNodeId, float _initialElapsed);
        EResult beginLocalAnimation(FigmaStringView _sourceNodeId, FigmaStringView _fromNodeId, const CanvasNodeDesc & _targetNode, const PrototypeActionDesc & _action, float _initialElapsed);
        void completeAnimation();
        void completeLocalAnimation(LocalAnimationState * const _animation);
        void updateAnimation(float _dt);
        void updateLocalAnimations(float _dt);
        void updatePrototypeTimers(float _dt);
        bool updatePrototypeTimersForNode(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame, FigmaStringView _sourceNodeId, float _startedAt, float _dt);
        void rebuildRenderList();
        void appendCanvasNode(const CanvasNodeDesc & _node, float _parentOpacity, float _offsetX, float _offsetY, const AnimationRenderContext * _animation, bool _renderLayerEnabled = true);
        void appendPrototypeHotspots(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame);
        void rebuildHotspots();
        void collectSmartAnimateTracks(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetFrame);
        void collectSmartAnimateTracksForPair(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect);
        const NodeSwapState * findNodeSwapState(FigmaStringView _nodeId) const;
        const LocalAnimationState * findLocalAnimation(FigmaStringView _nodeId) const;
        BindingValue resolveBindingValue(const BindingDesc & _item);
        bool isNodeVisibleByBinding(const CanvasNodeDesc & _node);
        bool isNodeEnabledByBinding(const CanvasNodeDesc & _node);
        bool resolveTextBinding(const CanvasNodeDesc & _node, FigmaString * const _text);
        bool resolveImageBinding(const CanvasNodeDesc & _node, FigmaString * const _assetId);
        const CanvasNodeDesc * resolveInitialFrame() const;

        Document & m_document;
        PlayerDesc m_desc;
        FigmaMemoryResource * m_memory;
        ActionRouterInterface * m_actionRouter = nullptr;
        DataContextInterface * m_dataContext = nullptr;
        RenderList m_renderList;
        Diagnostics m_diagnostics;
        HotspotVector m_hotspots;
        NodeIdSet m_hoveredNodeIds;
        NodeSwapStateMap m_nodeSwaps;
        LocalAnimationStateVector m_localAnimations;
        BindingOverrideMap m_overrides;
        FigmaString m_currentFrameId;
        PlayerAnimationStateDesc m_animationState;
        float m_time = 0.0f;
    };
}
