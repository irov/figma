#pragma once

#include "Figma/Types.h"

namespace Figma
{
    enum class ERenderBatchType
    {
        Geometry,
        ClipBegin,
        ClipEnd
    };

    enum class ERenderBlendMode
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

    enum class ERenderShaderType
    {
        Color,
        Texture,
        Debug
    };

    enum class ERenderTextureType
    {
        None,
        Asset,
        Generated
    };

    enum class ERenderTextAlignHorizontal
    {
        Left,
        Center,
        Right
    };

    enum class ERenderTextAlignVertical
    {
        Top,
        Center,
        Bottom
    };

    struct RenderVertex
    {
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        Color color;
    };

    struct RenderBatchDesc
    {
        ERenderBatchType batchType = ERenderBatchType::Geometry;
        ERenderShaderType shaderType = ERenderShaderType::Color;
        ERenderTextureType textureType = ERenderTextureType::None;
        FigmaStringView textureKey;
        ERenderBlendMode blendMode = ERenderBlendMode::Normal;
        float opacity = 1.0f;
        std::uint32_t renderLayerId = 0;
        float renderLayerOpacity = 1.0f;
        float filterColorAdjust[8] = {};
        float paintFilter[10] = {};
        Rectf clipRect = {0.0f, 0.0f, 0.0f, 0.0f};
        std::uint32_t vertexCount = 0;
        const RenderVertex * vertices = nullptr;
        std::uint32_t indexCount = 0;
        const std::uint16_t * indices = nullptr;
        bool hasFilterColorAdjustValue = false;
        bool hasPaintFilterValue = false;
    };

    struct RenderGeneratedTextLineDesc
    {
        FigmaStringView text;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float lineHeight = 0.0f;
        float lineAscent = 0.0f;
    };

    struct RenderGeneratedTextureDesc
    {
        FigmaStringView key;
        FigmaStringView text;
        FigmaStringView fontFamily;
        FigmaStringView fontStyle;
        FigmaStringView fontPostscriptName;
        Rectf rect = {0.0f, 0.0f, 0.0f, 0.0f};
        Color color = {1.0f, 1.0f, 1.0f, 1.0f};
        ERenderTextAlignHorizontal textAlignHorizontal = ERenderTextAlignHorizontal::Left;
        ERenderTextAlignVertical textAlignVertical = ERenderTextAlignVertical::Top;
        float fontSize = 18.0f;
        float lineHeight = 0.0f;
        int fontWeight = 400;
        std::uint32_t textLineCount = 0;
    };

    class RenderListInterface
    {
    public:
        virtual std::uint32_t getBatchCount() const = 0;
        virtual EResult getBatch(std::uint32_t _index, RenderBatchDesc * const _batch) const = 0;
        virtual EResult getGeneratedTexture(std::uint32_t _index, RenderGeneratedTextureDesc * const _desc) const = 0;
        virtual EResult getGeneratedTextureTextLine(std::uint32_t _index, std::uint32_t _lineIndex, RenderGeneratedTextLineDesc * const _line) const = 0;

    protected:
        ~RenderListInterface() = default;
    };
}
