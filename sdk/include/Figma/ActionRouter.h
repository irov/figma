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
        Key
    };

    struct ActionEvent
    {
        EActionInputKind inputKind = EActionInputKind::Pointer;
        FigmaStringView actionId;
        FigmaStringView sourceNodeId;
        FigmaStringView currentFrameId;
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
        virtual EResult routeAction(const ActionEvent & _event, ActionResponse * const _response) = 0;

    protected:
        ~ActionRouterInterface() = default;
    };
}
