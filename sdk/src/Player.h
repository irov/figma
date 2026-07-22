#pragma once

#include "Figma/PlayerInterface.h"
#include "Figma/ActionRouter.h"

#include "Diagnostics.h"
#include "Document.h"
#include "RenderList.h"

namespace Figma
{
    EResult createPlayerFromDocument( DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player );

    class Player final
        : public PlayerInterface
    {
    public:
        Player(Document & _document, const PlayerDesc & _desc, FigmaMemoryResource * _memory);
        ~Player();

        EResult setActionRouter(ActionRouterInterface * _router) override;
        EResult setDataContext(DataContextInterface * _context) override;
        EResult setViewport(const ViewportDesc & _viewport) override;
        EResult hitTest(float _x, float _y, bool * const _hit) const override;
        EResult inputPointer(const PointerEvent & _event, InputDispatchResult * const _dispatch) override;
        EResult inputKey(const KeyEvent & _event, InputDispatchResult * const _dispatch) override;
        EResult update(float _dt) override;
        EResult restart() override;
        EResult navigateToFrame(FigmaStringView _targetFrameId) override;
        EResult openOverlay(FigmaStringView _targetFrameId) override;
        EResult closeOverlay() override;
        EResult goBack() override;

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

    public:
        void destroy() override;

    protected:
        struct Hotspot
        {
            explicit Hotspot(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            Rectf rect{};
            Vec2f quad[4]{};
            Rectf clip{};
            FigmaString nodeId;
            FigmaString actionId;
            FigmaString targetFrameId;
            const PrototypeInteractionDesc * interaction = nullptr;
            const PrototypeActionDesc * prototypeAction = nullptr;
            EPrototypeEventType eventType = EPrototypeEventType::Click;
            std::uint32_t keyCode = 0;
            bool hasClip = false;
            bool uxAction = false;
        };

        using HotspotVector = FigmaVector<Hotspot>;

        struct AnimatedNodeDesc
        {
            Rectf rect{};
            Vec2f quad[4]{};
            CanvasArcDataDesc arcData{};
            float opacity = 1.0f;
            bool hasRect = false;
            bool hasQuad = false;
            bool hasArcData = false;
            bool hasOpacity = false;
            bool matched = false;
        };

        using AnimatedNodeMap = FigmaUnorderedMap<FigmaString, AnimatedNodeDesc>;
        using NodeIdSet = FigmaUnorderedSet<FigmaString>;
        using PersistentSourceNodeMap = FigmaUnorderedMap<FigmaString, const CanvasNodeDesc *>;

        struct AnimationRenderContext
        {
            explicit AnimationRenderContext(FigmaMemoryResource * _memory = getDefaultMemoryResource());

            AnimatedNodeMap targetNodes;
            NodeIdSet matchedNodeIds;
            NodeIdSet skipNodeIds;
            NodeIdSet opaqueNodeIds;
            PersistentSourceNodeMap persistentSourceNodes;
            FigmaString rootNodeId;
            float progress = 0.0f;
            bool targetPass = false;
            bool smartAnimate = false;
            bool skipRootGeometry = false;
            bool preserveNodeSwapState = false;
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
            AnimationTrackVector tracks;
            float elapsed = 0.0f;
            float duration = 0.0f;
            float progress = 0.0f;
            bool smartAnimate = false;
            bool active = false;
        };

        using NodeSwapStateMap = FigmaUnorderedMap<FigmaString, NodeSwapState>;
        using LocalAnimationStateVector = FigmaVector<LocalAnimationState>;
        using BindingOverrideMap = FigmaUnorderedMap<FigmaString, BindingValue>;

        struct PointerCapture
        {
            explicit PointerCapture(FigmaMemoryResource * _memory = getDefaultMemoryResource())
                : nodeId(_memory)
                , interactionId(_memory)
            {
            }

            FigmaString nodeId;
            FigmaString interactionId;
            EPointerButton button = EPointerButton::None;
        };

        using PointerCaptureMap = FigmaUnorderedMap<std::uint32_t, PointerCapture>;
        using FrameVector = FigmaVector<const CanvasNodeDesc *>;

        const Hotspot * findHotspot(float _x, float _y, EPrototypeEventType _eventType) const;
        EResult routeHotspot(const Hotspot & _hotspot, EActionInputKind _inputKind, const PointerEvent * _pointer, const KeyEvent * _key, float _initialElapsed = 0.0f);
        EResult routePrototypeAction(const Hotspot & _hotspot, const PrototypeActionDesc & _action, EActionInputKind _inputKind, const PointerEvent * _pointer, const KeyEvent * _key, float _initialElapsed);
        EResult executePrototypeAction(const PrototypeActionDesc & _action, FigmaStringView _sourceNodeId, float _initialElapsed);
        EResult executeActionResponse(const ActionResponse & _response, const PrototypeActionDesc * _action, FigmaStringView _sourceNodeId, FigmaStringView _defaultTargetFrameId, float _initialElapsed);
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
        void appendPrototypeHotspots(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame, float _offsetX, float _offsetY, const Rectf * _clip);
        void rebuildHotspots();
        void collectSmartAnimateTracks(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetFrame, AnimationTrackVector * const _tracks);
        void collectSmartAnimateTracksForPair(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect, AnimationTrackVector * const _tracks);
        const NodeSwapState * findNodeSwapState(FigmaStringView _nodeId) const;
        const LocalAnimationState * findLocalAnimation(FigmaStringView _nodeId) const;
        BindingValue resolveBindingValue(const BindingDesc & _item);
        bool isNodeVisibleByBinding(const CanvasNodeDesc & _node);
        bool isNodeEnabledByBinding(const CanvasNodeDesc & _node);
        bool resolveTextBinding(const CanvasNodeDesc & _node, FigmaString * const _text);
        bool resolveImageBinding(const CanvasNodeDesc & _node, FigmaString * const _assetId);
        void setCurrentFrame(const CanvasNodeDesc * _frame);
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
        NodeIdSet m_firedTimerInteractionIds;
        NodeSwapStateMap m_nodeSwaps;
        LocalAnimationStateVector m_localAnimations;
        BindingOverrideMap m_overrides;
        PointerCaptureMap m_pointerCaptures;
        FrameVector m_navigationHistory;
        FrameVector m_overlayFrames;
        FigmaVector<float> m_overlayStartTimes;
        const CanvasNodeDesc * m_currentFrame = nullptr;
        FigmaString m_currentFrameId;
        PlayerAnimationStateDesc m_animationState;
        float m_time = 0.0f;
        bool m_hotspotsDirty = true;
    };
}
