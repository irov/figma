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
        std::uint32_t vertexCount = 0;
        const RenderVertex * vertices = nullptr;
        std::uint32_t indexCount = 0;
        const std::uint16_t * indices = nullptr;
    };

    class RenderListInterface
    {
    public:
        virtual std::uint32_t getBatchCount() const = 0;
        virtual EResult getBatch(std::uint32_t _index, RenderBatchDesc * const _batch) const = 0;

    protected:
        ~RenderListInterface() = default;
    };
}
