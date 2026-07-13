#pragma once

#include "Figma/RenderListInterface.h"

namespace Figma
{
    struct RenderTextLineDesc;
    struct RenderCommand;

    enum class ERenderCommandType
    {
        Fill,
        Stroke,
        Image,
        Text,
        Mesh,
        ClipBegin,
        ClipEnd,
        DebugHotspot
    };

    enum class ERenderShapeType
    {
        Rectangle,
        RoundedRectangle,
        Ellipse
    };

    enum class ERenderImageScaleMode
    {
        Stretch,
        Fit,
        Fill,
        Tile,
        Unknown
    };

    using RenderTextLineVector = FigmaVector<RenderTextLineDesc>;
    using RenderVertexVector = FigmaVector<RenderVertex>;
    using RenderIndexVector = FigmaVector<std::uint16_t>;
    using RenderCommandVector = FigmaVector<RenderCommand>;

    struct RenderTextLineDesc
    {
        explicit RenderTextLineDesc(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        FigmaString text;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float lineHeight = 0.0f;
        float lineAscent = 0.0f;
    };

    struct RenderCommand final
    {
        explicit RenderCommand(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        ERenderCommandType type = ERenderCommandType::Fill;
        FigmaString id;
        FigmaString nodeId;
        FigmaString assetId;
        FigmaString text;
        FigmaString fontFamily;
        FigmaString fontStyle;
        FigmaString fontPostscriptName;
        Rectf rect = {0.0f, 0.0f, 0.0f, 0.0f};
        Color color = {1.0f, 1.0f, 1.0f, 1.0f};
        ERenderShapeType shape = ERenderShapeType::Rectangle;
        ERenderTextAlignHorizontal textAlignHorizontal = ERenderTextAlignHorizontal::Left;
        ERenderTextAlignVertical textAlignVertical = ERenderTextAlignVertical::Top;
        ERenderBlendMode blendMode = ERenderBlendMode::Normal;
        ERenderImageScaleMode imageScaleMode = ERenderImageScaleMode::Fill;
        float cornerRadius = 0.0f;
        float fontSize = 18.0f;
        float lineHeight = 0.0f;
        int fontWeight = 400;
        float strokeWidth = 1.0f;
        float opacity = 1.0f;
        std::uint32_t renderLayerId = 0;
        float renderLayerOpacity = 1.0f;
        float arcStartingAngle = 0.0f;
        float arcEndingAngle = 0.0f;
        float arcInnerRadius = 0.0f;
        float imageTransform[6] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        float filterColorAdjust[8] = {};
        float paintFilter[10] = {};
        std::uint32_t originalImageWidth = 0;
        std::uint32_t originalImageHeight = 0;
        bool hasArcDataValue = false;
        bool hasImageTransformValue = false;
        bool hasFilterColorAdjustValue = false;
        bool hasPaintFilterValue = false;
        RenderTextLineVector textLines;
        RenderVertexVector vertices;
        RenderIndexVector indices;
    };

    class RenderList final
        : public RenderListInterface
    {
    public:
        explicit RenderList(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        std::uint32_t getBatchCount() const override;
        EResult getBatch(std::uint32_t _index, RenderBatchDesc * const _batch) const override;
        EResult getGeneratedTexture(std::uint32_t _index, RenderGeneratedTextureDesc * const _desc) const override;
        EResult getGeneratedTextureTextLine(std::uint32_t _index, std::uint32_t _lineIndex, RenderGeneratedTextLineDesc * const _line) const override;

        void clear();
        RenderCommand & addCommand(ERenderCommandType _type);
        void removeLastCommand();
        const RenderCommandVector & getCommands() const;

    protected:
        FigmaMemoryResource * m_memory;
        RenderCommandVector m_commands;
    };
}
