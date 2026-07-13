#pragma once

#include "Figma/DataContext.h"
#include "Figma/DiagnosticsInterface.h"
#include "Figma/RenderListInterface.h"
#include "Figma/Types.h"

namespace Figma
{
    class ActionRouterInterface;

    enum class EPointerEventType
    {
        Down,
        Move,
        Up,
        Cancel
    };

    enum class EPointerButton
    {
        None,
        Left,
        Right,
        Middle,
        Other
    };

    enum class EInputModifierFlag : std::uint32_t
    {
        None = 0,
        Shift = 1 << 0,
        Control = 1 << 1,
        Alt = 1 << 2,
        Command = 1 << 3
    };

    using InputModifierFlags = EInputModifierFlag;

    constexpr InputModifierFlags operator | (InputModifierFlags _left, InputModifierFlags _right)
    {
        return static_cast<InputModifierFlags>(static_cast<std::uint32_t>(_left) | static_cast<std::uint32_t>(_right));
    }

    constexpr InputModifierFlags operator & (InputModifierFlags _left, InputModifierFlags _right)
    {
        return static_cast<InputModifierFlags>(static_cast<std::uint32_t>(_left) & static_cast<std::uint32_t>(_right));
    }

    inline InputModifierFlags & operator |= (InputModifierFlags & _left, InputModifierFlags _right)
    {
        _left = _left | _right;
        return _left;
    }

    constexpr bool hasInputModifier(InputModifierFlags _flags, EInputModifierFlag _flag)
    {
        return (_flags & _flag) != EInputModifierFlag::None;
    }

    struct PointerEvent
    {
        EPointerEventType type = EPointerEventType::Move;
        std::uint32_t pointerId = 0;
        float x = 0.0f;
        float y = 0.0f;
        EPointerButton button = EPointerButton::None;
        InputModifierFlags modifiers = EInputModifierFlag::None;
    };

    enum class EKeyEventType
    {
        Down,
        Up
    };

    struct KeyEvent
    {
        EKeyEventType type = EKeyEventType::Down;
        std::uint32_t keyCode = 0;
        InputModifierFlags modifiers = EInputModifierFlag::None;
    };

    struct ViewportDesc
    {
        float width = 1024.0f;
        float height = 768.0f;
        float scale = 1.0f;
    };

    struct PlayerDesc
    {
        ViewportDesc viewport;
        const Char * startFrameId = nullptr;
        const void * ud = nullptr;
    };

    struct InputDispatchResult
    {
        bool hit = false;
        bool handled = false;
        bool captured = false;
    };

    class PlayerInterface
    {
    public:
        virtual EResult setActionRouter(ActionRouterInterface * _router) = 0;
        virtual EResult setDataContext(DataContextInterface * _context) = 0;
        virtual EResult setViewport(const ViewportDesc & _viewport) = 0;
        virtual EResult inputPointer(const PointerEvent & _event, InputDispatchResult * const _dispatch = nullptr) = 0;
        virtual EResult inputKey(const KeyEvent & _event, InputDispatchResult * const _dispatch = nullptr) = 0;
        virtual EResult update(float _dt) = 0;
        virtual EResult restart() = 0;

        virtual EResult setText(FigmaStringView _key, FigmaStringView _value) = 0;
        virtual EResult setNumber(FigmaStringView _key, double _value) = 0;
        virtual EResult setVisible(FigmaStringView _key, bool _value) = 0;
        virtual EResult setEnabled(FigmaStringView _key, bool _value) = 0;
        virtual EResult setImage(FigmaStringView _key, FigmaStringView _assetId) = 0;
        virtual EResult setState(FigmaStringView _key, bool _value) = 0;
        virtual EResult setBindingValue(FigmaStringView _key, const BindingValue & _value) = 0;
        virtual EResult clearBindingValue(FigmaStringView _key) = 0;

        virtual const RenderListInterface * getRenderList() const = 0;
        virtual const DiagnosticsInterface * getDiagnostics() const = 0;

    public:
        virtual void destroy() = 0;

        virtual EResult hitTest(float _x, float _y, bool * const _hit) const = 0;
        virtual EResult navigateToFrame(FigmaStringView _targetFrameId) = 0;
        virtual EResult openOverlay(FigmaStringView _targetFrameId) = 0;
        virtual EResult closeOverlay() = 0;
        virtual EResult goBack() = 0;

    protected:
        ~PlayerInterface() = default;
    };
}
