#pragma once

#include "Figma/PlayerInterface.h"
#include "Figma/Types.h"

namespace Figma
{
    enum class EActionResult
    {
        AllowDefault,
        Consume,
        NavigateFrame,
        OpenOverlay,
        CloseOverlay
    };

    enum class EActionInputKind
    {
        Pointer,
        Key,
        Timer,
        Programmatic
    };

    enum class ETriggerType
    {
        Click,
        HoverEnter,
        HoverLeave,
        Press,
        PointerDown,
        PointerUp,
        AfterTimeout,
        KeyDown,
        Unsupported
    };

    enum class EConnectionType
    {
        None,
        InternalNode,
        Back,
        Close,
        Unsupported
    };

    enum class ENavigationType
    {
        Navigate,
        Overlay,
        Swap,
        ScrollTo,
        Unsupported
    };

    struct TriggerEvent
    {
        EActionInputKind inputKind = EActionInputKind::Pointer;
        ETriggerType triggerType = ETriggerType::Click;
        FigmaStringView interactionId;
        FigmaStringView sourceNodeId;
        FigmaStringView currentFrameId;
        PointerEvent pointer;
        KeyEvent key;
        const void * ud = nullptr;
    };

    struct ActionEvent
    {
        EActionInputKind inputKind = EActionInputKind::Pointer;
        ETriggerType triggerType = ETriggerType::Click;
        EConnectionType connectionType = EConnectionType::None;
        ENavigationType navigationType = ENavigationType::Navigate;
        FigmaStringView actionId;
        FigmaStringView interactionId;
        FigmaStringView sourceNodeId;
        FigmaStringView currentFrameId;
        FigmaStringView targetFrameId;
        PointerEvent pointer;
        KeyEvent key;
        const void * ud = nullptr;
    };

    struct ActionResponse
    {
        EActionResult result = EActionResult::AllowDefault;
        FigmaStringView targetFrameId;
    };

    class ActionRouterInterface
    {
    public:
        virtual EResult routeTrigger(const TriggerEvent & _event)
        {
            (void)_event;
            return EResult::Ok;
        }

        virtual EResult routeAction(const ActionEvent & _event, ActionResponse * const _response) = 0;

        virtual void onFrameChanged(FigmaStringView _previousFrameId, FigmaStringView _currentFrameId)
        {
            (void)_previousFrameId;
            (void)_currentFrameId;
        }

        virtual void onOverlayOpened(FigmaStringView _frameId)
        {
            (void)_frameId;
        }

        virtual void onOverlayClosed(FigmaStringView _frameId)
        {
            (void)_frameId;
        }

        virtual void onStateChanged(FigmaStringView _sourceNodeId, FigmaStringView _previousStateId, FigmaStringView _currentStateId)
        {
            (void)_sourceNodeId;
            (void)_previousStateId;
            (void)_currentStateId;
        }

    protected:
        ~ActionRouterInterface() = default;
    };
}
