#pragma once

#include "DocumentInspection.h"

namespace Figma
{
    struct CanvasPaint final
    {
        explicit CanvasPaint( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

        ECanvasPaintType type = ECanvasPaintType::Solid;
        ECanvasBlendMode blendMode = ECanvasBlendMode::Normal;
        ECanvasImageScaleMode imageScaleMode = ECanvasImageScaleMode::Fill;
        Colorf color;
        float opacity = 1.0f;
        bool visible = true;
        FigmaString assetId;
        FigmaString rawType;
        FigmaString rawBlendMode;
        float transform[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };
        float filterColorAdjust[8] = {};
        float paintFilter[10] = {};
        std::uint32_t originalImageWidth = 0;
        std::uint32_t originalImageHeight = 0;
        bool hasTransformValue = false;
        bool hasFilterColorAdjustValue = false;
        bool hasPaintFilterValue = false;
    };
}
