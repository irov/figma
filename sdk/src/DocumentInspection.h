#pragma once

#include "Figma/DocumentInterface.h"

namespace Figma
{
    struct CanvasPaint;
    struct CanvasPathDesc;
    struct CanvasNodeDesc;
    struct PrototypeActionDesc;
    struct PrototypeInteractionDesc;
    struct CanvasTextLineDesc;

    enum class EBindingProperty
    {
        Text,
        Visible,
        Enabled,
        Selected,
        Image
    };

    enum class ECanvasNodeType
    {
        Unknown,
        Document,
        Canvas,
        Frame,
        Group,
        Rectangle,
        RoundedRectangle,
        Ellipse,
        Text,
        Vector
    };

    enum class ECanvasPaintType
    {
        Unsupported,
        Solid,
        Image
    };

    enum class ECanvasBlendMode
    {
        PassThrough,
        Normal,
        Multiply,
        Screen,
        Overlay,
        Darken,
        Lighten,
        ColorDodge,
        ColorBurn,
        SoftLight,
        HardLight,
        Difference,
        Exclusion,
        Hue,
        Saturation,
        Color,
        Luminosity,
        Unsupported
    };

    enum class ECanvasImageScaleMode
    {
        Stretch,
        Fit,
        Fill,
        Tile,
        Unknown
    };

    enum class ECanvasMaskType
    {
        Alpha,
        Outline,
        Luminance,
        Unknown
    };

    enum class ECanvasStrokeAlign
    {
        Center,
        Inside,
        Outside,
        Unsupported
    };

    enum class ECanvasStrokeCap
    {
        None,
        Round,
        Square,
        Unsupported
    };

    enum class ECanvasStrokeJoin
    {
        Miter,
        Bevel,
        Round,
        Unsupported
    };

    enum class EPrototypeEventType
    {
        Click,
        Hover,
        AfterTimeout,
        Unsupported
    };

    enum class EPrototypeConnectionType
    {
        None,
        InternalNode,
        Back,
        Close,
        Unsupported
    };

    enum class EPrototypeNavigationType
    {
        Navigate,
        Overlay,
        Swap,
        ScrollTo,
        Unsupported
    };

    enum class EPrototypeTransitionType
    {
        Instant,
        Dissolve,
        SmartAnimate,
        MoveIn,
        MoveOut,
        Push,
        SlideIn,
        SlideOut,
        Unsupported
    };

    enum class EPrototypeTransitionDirection
    {
        None,
        Left,
        Right,
        Up,
        Down,
        Unsupported
    };

    enum class EAnimationTrackType
    {
        Opacity,
        Transform,
        Rect,
        Color,
        Visibility
    };

    enum class EAnimationEasing
    {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
        InCubic,
        OutCubic,
        InOutCubic,
        Unsupported
    };

    enum class EAnimationSource
    {
        None,
        PrototypeTransition,
        SmartAnimate
    };

    enum class ECanvasTextAlignHorizontal
    {
        Left,
        Center,
        Right
    };

    enum class ECanvasTextAlignVertical
    {
        Top,
        Center,
        Bottom
    };

    enum class ECanvasWindingRule
    {
        NonZero,
        Odd
    };

    enum class ECanvasPathCommandType
    {
        MoveTo,
        LineTo,
        QuadraticTo,
        CubicTo,
        Close
    };

    struct AnimationTrackDesc;
    struct BindingDesc;
    struct ActionDesc;

    using AnimationTrackVector = FigmaVector<AnimationTrackDesc>;
    using BindingVector = FigmaVector<BindingDesc>;
    using ActionVector = FigmaVector<ActionDesc>;

    struct CanvasPathCommandDesc
    {
        ECanvasPathCommandType type = ECanvasPathCommandType::MoveTo;
        Vec2f p0;
        Vec2f p1;
        Vec2f p2;
    };

    struct CanvasArcDataDesc
    {
        float startingAngle = 0.0f;
        float endingAngle = 0.0f;
        float innerRadius = 0.0f;
        bool valid = false;
    };

    struct AnimationTrackDesc
    {
        explicit AnimationTrackDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        FigmaString nodeId;
        FigmaString targetNodeId;
        EAnimationTrackType type = EAnimationTrackType::Opacity;
        float from[4] = {};
        float to[4] = {};
    };

    struct AnimationClipDesc
    {
        explicit AnimationClipDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        FigmaString id;
        FigmaString sourceFrameId;
        FigmaString targetFrameId;
        FigmaString sourceNodeId;
        EAnimationSource source = EAnimationSource::None;
        EPrototypeTransitionType transitionType = EPrototypeTransitionType::Instant;
        EPrototypeTransitionDirection transitionDirection = EPrototypeTransitionDirection::None;
        EAnimationEasing easing = EAnimationEasing::EaseInOut;
        float duration = 0.0f;
        bool smartAnimate = false;
        AnimationTrackVector tracks;
    };

    struct PlayerAnimationStateDesc
    {
        explicit PlayerAnimationStateDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        AnimationClipDesc clip;
        float elapsed = 0.0f;
        float progress = 0.0f;
        bool active = false;
    };

    class DocumentInspectionInterface
    {
    public:
        virtual const CanvasNodeDesc * getCanvasRoot() const = 0;
        virtual const CanvasNodeDesc * findCanvasNode(FigmaStringView _nodeId) const = 0;
        virtual const CanvasNodeDesc * getPrototypeStartFrame() const = 0;

    protected:
        ~DocumentInspectionInterface() = default;
    };

    struct BindingDesc
    {
        explicit BindingDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        FigmaString nodeId;
        FigmaString key;
        EBindingProperty property = EBindingProperty::Text;
    };

    struct ActionDesc
    {
        explicit ActionDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        FigmaString nodeId;
        FigmaString actionId;
        FigmaString targetFrameId;
    };

}
