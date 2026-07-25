#pragma once

#include "figma/figma.hpp"

extern "C"
{
#include "figma_inspection.h"
}

#include <cstdint>
#include <string>
#include <vector>

struct ViewerRenderTextLineDesc
{
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float lineHeight = 0.0f;
    float lineAscent = 0.0f;
};

struct ViewerRenderCommand
{
    figma_render_command_type_t type = FIGMA_RENDER_COMMAND_FILL;
    std::string id;
    std::string nodeId;
    std::string assetId;
    std::string text;
    std::string fontFamily;
    std::string fontStyle;
    std::string fontPostscriptName;
    figma_rectf_t rect = {0.0f, 0.0f, 0.0f, 0.0f};
    figma_colorf_t color = {1.0f, 1.0f, 1.0f, 1.0f};
    figma_render_shape_type_t shape = FIGMA_RENDER_SHAPE_RECTANGLE;
    figma_render_text_align_horizontal_t textAlignHorizontal =
        FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_LEFT;
    figma_render_text_align_vertical_t textAlignVertical =
        FIGMA_RENDER_TEXT_ALIGN_VERTICAL_TOP;
    figma_render_blend_mode_t blendMode = FIGMA_RENDER_BLEND_NORMAL;
    figma_render_image_scale_mode_t imageScaleMode =
        FIGMA_RENDER_IMAGE_SCALE_FILL;
    float cornerRadius = 0.0f;
    float fontSize = 18.0f;
    float lineHeight = 0.0f;
    int32_t fontWeight = 400;
    float strokeWidth = 1.0f;
    float opacity = 1.0f;
    uint32_t renderLayerId = 0u;
    float renderLayerOpacity = 1.0f;
    float arcStartingAngle = 0.0f;
    float arcEndingAngle = 0.0f;
    float arcInnerRadius = 0.0f;
    float imageTransform[6] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float filterColorAdjust[8] = {};
    float paintFilter[10] = {};
    uint32_t originalImageWidth = 0u;
    uint32_t originalImageHeight = 0u;
    bool hasArcDataValue = false;
    bool hasImageTransformValue = false;
    bool hasFilterColorAdjustValue = false;
    bool hasPaintFilterValue = false;
    std::vector<ViewerRenderTextLineDesc> textLines;
    std::vector<figma_render_vertex_t> vertices;
    std::vector<uint16_t> indices;
};

using ViewerRenderCommandVector = std::vector<ViewerRenderCommand>;

bool copyViewerRenderCommands(
    const figma_render_list_t * renderList,
    ViewerRenderCommandVector * commands);
