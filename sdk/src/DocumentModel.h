#pragma once

#include "CanvasPaint.h"

namespace Figma
{
    struct CanvasPathStyleOverrideDesc;
    struct CanvasPathDesc;
    struct PrototypeActionDesc;
    struct PrototypeInteractionDesc;
    struct CanvasTextLineDesc;
    struct CanvasNodeDesc;

    using CanvasPathCommandVector = FigmaVector<CanvasPathCommandDesc>;
    using CanvasPaintVector = FigmaVector<CanvasPaint>;
    using CanvasPathStyleOverrideVector = FigmaVector<CanvasPathStyleOverrideDesc>;
    using CanvasPathVector = FigmaVector<CanvasPathDesc>;
    using PrototypeActionVector = FigmaVector<PrototypeActionDesc>;
    using PrototypeInteractionVector = FigmaVector<PrototypeInteractionDesc>;
    using CanvasTextLineVector = FigmaVector<CanvasTextLineDesc>;
    using CanvasNodeVector = FigmaVector<CanvasNodeDesc>;
    using UnsupportedFieldVector = FigmaVector<FigmaString>;
    using DashPatternVector = FigmaVector<float>;

    template <class TContainer> const typename TContainer::value_type * valueAt( const TContainer & _values, std::uint32_t _index )
    {
        if( _index >= _values.size() )
        {
            return nullptr;
        }

        return &_values[_index];
    }

    struct CanvasPathStyleOverrideDesc
    {
        explicit CanvasPathStyleOverrideDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        std::uint32_t styleId = 0;
        CanvasPaintVector fills;
        CanvasPaintVector strokes;
    };

    struct CanvasPathDesc final
    {
        explicit CanvasPathDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        ECanvasWindingRule windingRule = ECanvasWindingRule::NonZero;
        std::uint32_t commandsBlob = 0;
        std::uint32_t styleId = 0;
        bool commandsDecoded = false;
        CanvasPathCommandVector commands;
        CanvasPaintVector paints;
    };

    struct PrototypeActionDesc final
    {
        explicit PrototypeActionDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        FigmaString targetNodeId;
        FigmaString rawConnectionType;
        FigmaString rawNavigationType;
        FigmaString rawTransitionType;
        FigmaString rawTransitionDirection;
        FigmaString rawTransitionEasing;
        EPrototypeConnectionType connectionType = EPrototypeConnectionType::None;
        EPrototypeNavigationType navigationType = EPrototypeNavigationType::Navigate;
        EPrototypeTransitionType transitionType = EPrototypeTransitionType::Instant;
        EPrototypeTransitionDirection transitionDirection = EPrototypeTransitionDirection::None;
        EAnimationEasing transitionEasing = EAnimationEasing::EaseInOut;
        float transitionDuration = 0.0f;
        bool smartAnimate = false;
        bool transitionPreserveScroll = false;
        bool transitionResetVideoPosition = false;
        bool hasEasingFunctionValue = false;
        UnsupportedFieldVector unsupportedFields;
    };

    struct PrototypeInteractionDesc final
    {
        explicit PrototypeInteractionDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        FigmaString id;
        FigmaString rawEventType;
        EPrototypeEventType eventType = EPrototypeEventType::Unsupported;
        float transitionTimeout = 0.0f;
        std::uint32_t keyCode = 0;
        PrototypeActionVector actions;
        UnsupportedFieldVector unsupportedFields;
    };

    struct CanvasTextLineDesc final
    {
        explicit CanvasTextLineDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        FigmaString text;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float lineHeight = 0.0f;
        float lineAscent = 0.0f;
    };

    struct CanvasNodeDesc final
    {
        explicit CanvasNodeDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        FigmaString id;
        FigmaString name;
        ECanvasNodeType type = ECanvasNodeType::Unknown;
        Vec2f size;
        Rectf rect;
        Vec2f quad[4];
        float opacity = 1.0f;
        float cornerRadius = 0.0f;
        float strokeWeight = 1.0f;
        float fontSize = 18.0f;
        float lineHeight = 0.0f;
        int fontWeight = 400;
        bool visible = true;
        bool mask = false;
        bool frameMaskDisabled = false;
        bool hasFillGeometryValue = false;
        bool hasStrokeGeometryValue = false;
        bool hasVectorDataValue = false;
        bool hasVectorNetworkBlobValue = false;
        bool hasPrototypeStartingPointValue = false;
        std::uint32_t vectorNetworkBlob = 0;
        std::uint32_t prototypeInteractionCount = 0;
        Vec2f vectorNormalizedSize;
        ECanvasMaskType maskType = ECanvasMaskType::Alpha;
        ECanvasBlendMode blendMode = ECanvasBlendMode::Normal;
        ECanvasStrokeAlign strokeAlign = ECanvasStrokeAlign::Center;
        ECanvasStrokeCap strokeCap = ECanvasStrokeCap::None;
        ECanvasStrokeJoin strokeJoin = ECanvasStrokeJoin::Miter;
        CanvasArcDataDesc arcData;
        ECanvasTextAlignHorizontal textAlignHorizontal = ECanvasTextAlignHorizontal::Left;
        ECanvasTextAlignVertical textAlignVertical = ECanvasTextAlignVertical::Top;
        FigmaString text;
        FigmaString fontFamily;
        FigmaString fontStyle;
        FigmaString fontPostscriptName;
        FigmaString prototypeStartNodeId;
        FigmaString symbolId;
        FigmaString fillStyleNodeId;
        FigmaString strokeFillStyleNodeId;
        FigmaString rawBlendMode;
        DashPatternVector dashPattern;
        CanvasPathStyleOverrideVector pathStyleOverrides;
        CanvasPathVector fillGeometry;
        CanvasPathVector strokeGeometry;
        PrototypeInteractionVector prototypeInteractions;
        CanvasTextLineVector textLines;
        CanvasPaintVector fills;
        CanvasPaintVector strokes;
        CanvasNodeVector children;
    };
}
