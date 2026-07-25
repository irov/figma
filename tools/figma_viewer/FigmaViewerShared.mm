#include "FigmaViewerShared.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

//////////////////////////////////////////////////////////////////////////
static std::string viewerString(figma_string_view_t value)
{
    return value.data != nullptr
        ? std::string(value.data, value.size)
        : std::string();
}

//////////////////////////////////////////////////////////////////////////
bool copyViewerRenderCommands(
    const figma_render_list_t * renderList,
    ViewerRenderCommandVector * commands)
{
    if(renderList == nullptr || commands == nullptr)
    {
        return false;
    }

    const uint32_t count = figma_render_list_get_batch_count(renderList);
    ViewerRenderCommandVector copied;
    copied.reserve(count);
    for(uint32_t index = 0u; index != count; ++index)
    {
        figma_inspection_render_command_desc_t source = {};
        if(figma_inspection_get_render_command(renderList, index, &source) !=
            FIGMA_RESULT_OK)
        {
            return false;
        }

        ViewerRenderCommand command;
        command.type = source.type;
        command.id = viewerString(source.id);
        command.nodeId = viewerString(source.node_id);
        command.assetId = viewerString(source.asset_id);
        command.text = viewerString(source.text);
        command.fontFamily = viewerString(source.font_family);
        command.fontStyle = viewerString(source.font_style);
        command.fontPostscriptName =
            viewerString(source.font_postscript_name);
        command.rect = source.rect;
        command.color = source.color;
        command.shape = source.shape;
        command.textAlignHorizontal = source.text_align_horizontal;
        command.textAlignVertical = source.text_align_vertical;
        command.blendMode = source.blend_mode;
        command.imageScaleMode = source.image_scale_mode;
        command.cornerRadius = source.corner_radius;
        command.fontSize = source.font_size;
        command.lineHeight = source.line_height;
        command.fontWeight = source.font_weight;
        command.strokeWidth = source.stroke_width;
        command.opacity = source.opacity;
        command.renderLayerId = source.render_layer_id;
        command.renderLayerOpacity = source.render_layer_opacity;
        command.arcStartingAngle = source.arc_starting_angle;
        command.arcEndingAngle = source.arc_ending_angle;
        command.arcInnerRadius = source.arc_inner_radius;
        std::memcpy(
            command.imageTransform,
            source.image_transform,
            sizeof(command.imageTransform));
        std::memcpy(
            command.filterColorAdjust,
            source.filter_color_adjust,
            sizeof(command.filterColorAdjust));
        std::memcpy(
            command.paintFilter,
            source.paint_filter,
            sizeof(command.paintFilter));
        command.originalImageWidth = source.original_image_width;
        command.originalImageHeight = source.original_image_height;
        command.hasArcDataValue = source.has_arc_data != FIGMA_FALSE;
        command.hasImageTransformValue =
            source.has_image_transform != FIGMA_FALSE;
        command.hasFilterColorAdjustValue =
            source.has_filter_color_adjust != FIGMA_FALSE;
        command.hasPaintFilterValue =
            source.has_paint_filter != FIGMA_FALSE;
        if(source.vertex_count != 0u)
        {
            command.vertices.assign(
                source.vertices, source.vertices + source.vertex_count);
        }
        if(source.index_count != 0u)
        {
            command.indices.assign(
                source.indices, source.indices + source.index_count);
        }
        command.textLines.reserve(source.text_line_count);
        for(uint32_t lineIndex = 0u;
            lineIndex != source.text_line_count;
            ++lineIndex)
        {
            figma_render_generated_text_line_desc_t sourceLine = {};
            if(figma_render_list_get_generated_texture_text_line(
                   renderList, index, lineIndex, &sourceLine) !=
                FIGMA_RESULT_OK)
            {
                return false;
            }
            ViewerRenderTextLineDesc line;
            line.text = viewerString(sourceLine.text);
            line.x = sourceLine.x;
            line.y = sourceLine.y;
            line.width = sourceLine.width;
            line.lineHeight = sourceLine.line_height;
            line.lineAscent = sourceLine.line_ascent;
            command.textLines.emplace_back(std::move(line));
        }
        copied.emplace_back(std::move(command));
    }

    *commands = std::move(copied);
    return true;
}

//////////////////////////////////////////////////////////////////////////
void destroyFigmaObject(figma_player_t *& player)
{
    figma_player_destroy(player);
    player = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void destroyFigmaObject(figma_document_t *& document)
{
    figma_document_destroy(document);
    document = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void destroyFigmaObject(figma_runtime_t *& runtime)
{
    figma_runtime_destroy(runtime);
    runtime = nullptr;
}

//////////////////////////////////////////////////////////////////////////
const char * resultToString(figma_result_t _result)
{
    switch(_result)
    {
    case FIGMA_RESULT_OK:
        return "Ok";
    case FIGMA_RESULT_INVALID_ARGUMENT:
        return "InvalidArgument";
    case FIGMA_RESULT_OUT_OF_MEMORY:
        return "OutOfMemory";
    case FIGMA_RESULT_IO_FAILED:
        return "IoFailed";
    case FIGMA_RESULT_PARSE_FAILED:
        return "ParseFailed";
    case FIGMA_RESULT_UNSUPPORTED_FORMAT:
        return "UnsupportedFormat";
    case FIGMA_RESULT_MISSING_ENTRY:
        return "MissingEntry";
    case FIGMA_RESULT_NOT_FOUND:
        return "NotFound";
    case FIGMA_RESULT_INVALID_STATE:
        return "InvalidState";
    case FIGMA_RESULT_VERSION_MISMATCH:
        return "VersionMismatch";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
NSColor * colorFromCommand(const ViewerRenderCommand & _command, CGFloat _alpha)
{
    return [NSColor colorWithSRGBRed:_command.color.r
                                green:_command.color.g
                                 blue:_command.color.b
                                alpha:_command.color.a * _command.opacity * _alpha];
}

//////////////////////////////////////////////////////////////////////////
NSColor * colorFromVertex(const figma_render_vertex_t & _vertex)
{
    return [NSColor colorWithSRGBRed:_vertex.color.r
                                green:_vertex.color.g
                                 blue:_vertex.color.b
                                alpha:_vertex.color.a];
}

//////////////////////////////////////////////////////////////////////////
CGBlendMode cgBlendModeForCommand(const ViewerRenderCommand & _command)
{
    switch(_command.blendMode)
    {
    case FIGMA_RENDER_BLEND_MULTIPLY:
        return kCGBlendModeMultiply;
    case FIGMA_RENDER_BLEND_SCREEN:
        return kCGBlendModeScreen;
    case FIGMA_RENDER_BLEND_OVERLAY:
        return kCGBlendModeOverlay;
    case FIGMA_RENDER_BLEND_DARKEN:
        return kCGBlendModeDarken;
    case FIGMA_RENDER_BLEND_LIGHTEN:
        return kCGBlendModeLighten;
    case FIGMA_RENDER_BLEND_COLOR_DODGE:
        return kCGBlendModeColorDodge;
    case FIGMA_RENDER_BLEND_COLOR_BURN:
        return kCGBlendModeColorBurn;
    case FIGMA_RENDER_BLEND_SOFT_LIGHT:
        return kCGBlendModeSoftLight;
    case FIGMA_RENDER_BLEND_HARD_LIGHT:
        return kCGBlendModeHardLight;
    case FIGMA_RENDER_BLEND_DIFFERENCE:
        return kCGBlendModeDifference;
    case FIGMA_RENDER_BLEND_EXCLUSION:
        return kCGBlendModeExclusion;
    case FIGMA_RENDER_BLEND_HUE:
        return kCGBlendModeHue;
    case FIGMA_RENDER_BLEND_SATURATION:
        return kCGBlendModeSaturation;
    case FIGMA_RENDER_BLEND_COLOR:
        return kCGBlendModeColor;
    case FIGMA_RENDER_BLEND_LUMINOSITY:
        return kCGBlendModeLuminosity;
    case FIGMA_RENDER_BLEND_PASS_THROUGH:
    case FIGMA_RENDER_BLEND_NORMAL:
    case FIGMA_RENDER_BLEND_UNSUPPORTED:
        break;
    }

    return kCGBlendModeNormal;
}

//////////////////////////////////////////////////////////////////////////
NSCompositingOperation compositingOperationForCommand(const ViewerRenderCommand & _command)
{
    switch(_command.blendMode)
    {
    case FIGMA_RENDER_BLEND_MULTIPLY:
        return NSCompositingOperationMultiply;
    case FIGMA_RENDER_BLEND_SCREEN:
        return NSCompositingOperationScreen;
    case FIGMA_RENDER_BLEND_OVERLAY:
        return NSCompositingOperationOverlay;
    case FIGMA_RENDER_BLEND_DARKEN:
        return NSCompositingOperationDarken;
    case FIGMA_RENDER_BLEND_LIGHTEN:
        return NSCompositingOperationLighten;
    case FIGMA_RENDER_BLEND_COLOR_DODGE:
        return NSCompositingOperationColorDodge;
    case FIGMA_RENDER_BLEND_COLOR_BURN:
        return NSCompositingOperationColorBurn;
    case FIGMA_RENDER_BLEND_SOFT_LIGHT:
        return NSCompositingOperationSoftLight;
    case FIGMA_RENDER_BLEND_HARD_LIGHT:
        return NSCompositingOperationHardLight;
    case FIGMA_RENDER_BLEND_DIFFERENCE:
        return NSCompositingOperationDifference;
    case FIGMA_RENDER_BLEND_EXCLUSION:
        return NSCompositingOperationExclusion;
    case FIGMA_RENDER_BLEND_HUE:
        return NSCompositingOperationHue;
    case FIGMA_RENDER_BLEND_SATURATION:
        return NSCompositingOperationSaturation;
    case FIGMA_RENDER_BLEND_COLOR:
        return NSCompositingOperationColor;
    case FIGMA_RENDER_BLEND_LUMINOSITY:
        return NSCompositingOperationLuminosity;
    case FIGMA_RENDER_BLEND_PASS_THROUGH:
    case FIGMA_RENDER_BLEND_NORMAL:
    case FIGMA_RENDER_BLEND_UNSUPPORTED:
        break;
    }

    return NSCompositingOperationSourceOver;
}

//////////////////////////////////////////////////////////////////////////
const char * renderCommandTypeName(figma_render_command_type_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_COMMAND_FILL:
        return "Fill";
    case FIGMA_RENDER_COMMAND_STROKE:
        return "Stroke";
    case FIGMA_RENDER_COMMAND_IMAGE:
        return "Image";
    case FIGMA_RENDER_COMMAND_TEXT:
        return "Text";
    case FIGMA_RENDER_COMMAND_MESH:
        return "Mesh";
    case FIGMA_RENDER_COMMAND_CLIP_BEGIN:
        return "ClipBegin";
    case FIGMA_RENDER_COMMAND_CLIP_END:
        return "ClipEnd";
    case FIGMA_RENDER_COMMAND_DEBUG_HOTSPOT:
        return "Hotspot";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
const char * renderShapeTypeName(figma_render_shape_type_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_SHAPE_RECTANGLE:
        return "Rectangle";
    case FIGMA_RENDER_SHAPE_ROUNDED_RECTANGLE:
        return "RoundedRectangle";
    case FIGMA_RENDER_SHAPE_ELLIPSE:
        return "Ellipse";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
const char * renderTextAlignHorizontalName(figma_render_text_align_horizontal_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_LEFT:
        return "Left";
    case FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_CENTER:
        return "Center";
    case FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_RIGHT:
        return "Right";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
const char * renderTextAlignVerticalName(figma_render_text_align_vertical_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_TEXT_ALIGN_VERTICAL_TOP:
        return "Top";
    case FIGMA_RENDER_TEXT_ALIGN_VERTICAL_CENTER:
        return "Center";
    case FIGMA_RENDER_TEXT_ALIGN_VERTICAL_BOTTOM:
        return "Bottom";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
const char * renderBlendModeName(figma_render_blend_mode_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_BLEND_PASS_THROUGH:
        return "PassThrough";
    case FIGMA_RENDER_BLEND_NORMAL:
        return "Normal";
    case FIGMA_RENDER_BLEND_MULTIPLY:
        return "Multiply";
    case FIGMA_RENDER_BLEND_SCREEN:
        return "Screen";
    case FIGMA_RENDER_BLEND_OVERLAY:
        return "Overlay";
    case FIGMA_RENDER_BLEND_DARKEN:
        return "Darken";
    case FIGMA_RENDER_BLEND_LIGHTEN:
        return "Lighten";
    case FIGMA_RENDER_BLEND_COLOR_DODGE:
        return "ColorDodge";
    case FIGMA_RENDER_BLEND_COLOR_BURN:
        return "ColorBurn";
    case FIGMA_RENDER_BLEND_SOFT_LIGHT:
        return "SoftLight";
    case FIGMA_RENDER_BLEND_HARD_LIGHT:
        return "HardLight";
    case FIGMA_RENDER_BLEND_DIFFERENCE:
        return "Difference";
    case FIGMA_RENDER_BLEND_EXCLUSION:
        return "Exclusion";
    case FIGMA_RENDER_BLEND_HUE:
        return "Hue";
    case FIGMA_RENDER_BLEND_SATURATION:
        return "Saturation";
    case FIGMA_RENDER_BLEND_COLOR:
        return "Color";
    case FIGMA_RENDER_BLEND_LUMINOSITY:
        return "Luminosity";
    case FIGMA_RENDER_BLEND_UNSUPPORTED:
        return "Unsupported";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
const char * renderImageScaleModeName(figma_render_image_scale_mode_t _type)
{
    switch(_type)
    {
    case FIGMA_RENDER_IMAGE_SCALE_STRETCH:
        return "Stretch";
    case FIGMA_RENDER_IMAGE_SCALE_FIT:
        return "Fit";
    case FIGMA_RENDER_IMAGE_SCALE_FILL:
        return "Fill";
    case FIGMA_RENDER_IMAGE_SCALE_TILE:
        return "Tile";
    case FIGMA_RENDER_IMAGE_SCALE_UNKNOWN:
        return "Unknown";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
NSString * wireframeModeTitle(EViewerWireframeMode _mode)
{
    switch(_mode)
    {
    case EViewerWireframeMode::Normal:
        return @"Normal";
    case EViewerWireframeMode::Wireframe:
        return @"Wire";
    case EViewerWireframeMode::Combined:
        return @"Combined";
    }

    return @"Normal";
}

//////////////////////////////////////////////////////////////////////////
NSString * nsString(const std::string & _value)
{
    return [[NSString alloc] initWithBytes:_value.data() length:_value.size() encoding:NSUTF8StringEncoding] ?: @"";
}

//////////////////////////////////////////////////////////////////////////
NSString * nsString(figma_string_view_t value)
{
    return [[NSString alloc] initWithBytes:value.data
                                    length:value.size
                                  encoding:NSUTF8StringEncoding] ?: @"";
}

//////////////////////////////////////////////////////////////////////////
NSString * documentPath(const figma_document_t * document)
{
    figma_inspection_document_desc_t desc = {};
    return document != nullptr &&
            figma_inspection_get_document(document, &desc) == FIGMA_RESULT_OK
        ? nsString(desc.path)
        : @"";
}

//////////////////////////////////////////////////////////////////////////
NSString * documentFileName(const figma_document_t * document)
{
    figma_inspection_document_desc_t desc = {};
    return document != nullptr &&
            figma_inspection_get_document(document, &desc) == FIGMA_RESULT_OK
        ? nsString(desc.file_name)
        : @"";
}

//////////////////////////////////////////////////////////////////////////
NSString * escapedInspectorString(const std::string & _value)
{
    NSString * string = nsString(_value);
    string = [string stringByReplacingOccurrencesOfString:@"\n" withString:@"\\n"];
    string = [string stringByReplacingOccurrencesOfString:@"\r" withString:@"\\r"];
    string = [string stringByReplacingOccurrencesOfString:@"\t" withString:@"\\t"];
    return string;
}

//////////////////////////////////////////////////////////////////////////
NSString * inspectorBoolString(bool _value)
{
    return _value == true ? @"true" : @"false";
}

//////////////////////////////////////////////////////////////////////////
NSString * inspectorRectString(const figma_rectf_t & _rect)
{
    return [NSString stringWithFormat:@"x=%.3f y=%.3f w=%.3f h=%.3f", _rect.x, _rect.y, _rect.w, _rect.h];
}

//////////////////////////////////////////////////////////////////////////
NSString * inspectorColorString(const figma_colorf_t & _color)
{
    return [NSString stringWithFormat:@"r=%.3f g=%.3f b=%.3f a=%.3f", _color.r, _color.g, _color.b, _color.a];
}

//////////////////////////////////////////////////////////////////////////
NSString * inspectorFloatArrayString(const float * _values, std::size_t _count)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(4);
    stream << "[";
    for(std::size_t index = 0; index != _count; ++index)
    {
        if(index != 0)
        {
            stream << ", ";
        }

        stream << _values[index];
    }
    stream << "]";

    const std::string value = stream.str();
    return [NSString stringWithUTF8String:value.c_str()];
}

//////////////////////////////////////////////////////////////////////////
static CGFloat clampUnit(CGFloat _value)
{
    return std::max<CGFloat>(0.0, std::min<CGFloat>(1.0, _value));
}

struct HsvColor
{
    CGFloat h = 0.0;
    CGFloat s = 0.0;
    CGFloat v = 0.0;
};

//////////////////////////////////////////////////////////////////////////
static HsvColor rgbToHsv(CGFloat _red, CGFloat _green, CGFloat _blue)
{
    const CGFloat maxValue = std::max(_red, std::max(_green, _blue));
    const CGFloat minValue = std::min(_red, std::min(_green, _blue));
    const CGFloat delta = maxValue - minValue;

    HsvColor result;
    result.v = maxValue;
    result.s = maxValue > 0.0 ? delta / maxValue : 0.0;

    if(delta <= 0.000001)
    {
        result.h = 0.0;
        return result;
    }

    if(maxValue == _red)
    {
        result.h = (_green - _blue) / delta;
        if(result.h < 0.0)
        {
            result.h += 6.0;
        }
    }
    else if(maxValue == _green)
    {
        result.h = 2.0 + (_blue - _red) / delta;
    }
    else
    {
        result.h = 4.0 + (_red - _green) / delta;
    }

    result.h /= 6.0;
    return result;
}

//////////////////////////////////////////////////////////////////////////
static void hsvToRgb(const HsvColor & _hsv, CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue)
{
    if(_hsv.s <= 0.000001)
    {
        *_red = _hsv.v;
        *_green = _hsv.v;
        *_blue = _hsv.v;
        return;
    }

    const CGFloat hue = std::fmod(std::max<CGFloat>(0.0, _hsv.h), 1.0) * 6.0;
    const int sector = static_cast<int>(std::floor(hue));
    const CGFloat fraction = hue - static_cast<CGFloat>(sector);
    const CGFloat p = _hsv.v * (1.0 - _hsv.s);
    const CGFloat q = _hsv.v * (1.0 - _hsv.s * fraction);
    const CGFloat t = _hsv.v * (1.0 - _hsv.s * (1.0 - fraction));

    switch(sector)
    {
    case 0:
        *_red = _hsv.v;
        *_green = t;
        *_blue = p;
        break;
    case 1:
        *_red = q;
        *_green = _hsv.v;
        *_blue = p;
        break;
    case 2:
        *_red = p;
        *_green = _hsv.v;
        *_blue = t;
        break;
    case 3:
        *_red = p;
        *_green = q;
        *_blue = _hsv.v;
        break;
    case 4:
        *_red = t;
        *_green = p;
        *_blue = _hsv.v;
        break;
    default:
        *_red = _hsv.v;
        *_green = p;
        *_blue = q;
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
static void applyExposure(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _exposure)
{
    if(std::fabs(_exposure) <= 0.0001)
    {
        return;
    }

    HsvColor hsv = rgbToHsv(*_red, *_green, *_blue);
    const CGFloat value = std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _exposure));
    if(value > 0.0)
    {
        const CGFloat lift = value * 0.65;
        hsv.v = hsv.v + (1.0 - hsv.v) * lift;
        hsv.s *= 1.0 - value * 0.22;
    }
    else
    {
        hsv.v *= 1.0 + value;
    }

    hsv.v = clampUnit(hsv.v);
    hsv.s = clampUnit(hsv.s);
    hsvToRgb(hsv, _red, _green, _blue);
}

//////////////////////////////////////////////////////////////////////////
static void applyBrightness(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _brightness)
{
    if(std::fabs(_brightness) <= 0.0001)
    {
        return;
    }

    const CGFloat value = std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _brightness));
    *_red = clampUnit(*_red + value);
    *_green = clampUnit(*_green + value);
    *_blue = clampUnit(*_blue + value);
}

//////////////////////////////////////////////////////////////////////////
static void applyContrast(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _contrast)
{
    if(std::fabs(_contrast) <= 0.0001)
    {
        return;
    }

    const CGFloat factor = std::max<CGFloat>(0.0, 1.0 + std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _contrast)));
    *_red = clampUnit((*_red - 0.5) * factor + 0.5);
    *_green = clampUnit((*_green - 0.5) * factor + 0.5);
    *_blue = clampUnit((*_blue - 0.5) * factor + 0.5);
}

//////////////////////////////////////////////////////////////////////////
static void applyShadowsHighlights(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _shadows, CGFloat _highlights)
{
    if(std::fabs(_shadows) <= 0.0001 && std::fabs(_highlights) <= 0.0001)
    {
        return;
    }

    const CGFloat luminance = clampUnit(*_red * 0.2126 + *_green * 0.7152 + *_blue * 0.0722);
    const CGFloat shadowMask = (1.0 - luminance) * (1.0 - luminance);
    const CGFloat highlightMask = luminance * luminance;

    auto applyMaskedLift = [](CGFloat _component, CGFloat _amount, CGFloat _mask, CGFloat _scale) {
        const CGFloat value = std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _amount)) * _mask * _scale;
        if(value > 0.0)
        {
            return clampUnit(_component + (1.0 - _component) * value);
        }

        return clampUnit(_component * (1.0 + value));
    };

    *_red = applyMaskedLift(*_red, _shadows, shadowMask, 0.82);
    *_green = applyMaskedLift(*_green, _shadows, shadowMask, 0.82);
    *_blue = applyMaskedLift(*_blue, _shadows, shadowMask, 0.82);

    *_red = applyMaskedLift(*_red, _highlights, highlightMask, 0.62);
    *_green = applyMaskedLift(*_green, _highlights, highlightMask, 0.62);
    *_blue = applyMaskedLift(*_blue, _highlights, highlightMask, 0.62);
}

//////////////////////////////////////////////////////////////////////////
static void applyTemperature(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _temperature)
{
    if(std::fabs(_temperature) <= 0.0001)
    {
        return;
    }

    const CGFloat value = std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _temperature));
    const CGFloat amount = std::fabs(value);
    if(value > 0.0)
    {
        *_red = clampUnit(*_red + (1.0 - *_red) * amount * 0.18);
        *_green = clampUnit(*_green + (1.0 - *_green) * amount * 0.035);
        *_blue = clampUnit(*_blue * (1.0 - amount * 0.16));
    }
    else
    {
        *_red = clampUnit(*_red * (1.0 - amount * 0.16));
        *_green = clampUnit(*_green + (1.0 - *_green) * amount * 0.025);
        *_blue = clampUnit(*_blue + (1.0 - *_blue) * amount * 0.18);
    }
}

//////////////////////////////////////////////////////////////////////////
static void applyVibrance(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _vibrance)
{
    if(std::fabs(_vibrance) <= 0.0001)
    {
        return;
    }

    HsvColor hsv = rgbToHsv(*_red, *_green, *_blue);
    const CGFloat value = std::max<CGFloat>(-1.0, std::min<CGFloat>(1.0, _vibrance));
    if(value > 0.0)
    {
        const CGFloat lowSaturationWeight = 1.0 - hsv.s;
        hsv.s += (1.0 - hsv.s) * value * (0.55 + lowSaturationWeight * 0.45);
    }
    else
    {
        hsv.s *= 1.0 + value;
    }

    hsv.s = clampUnit(hsv.s);
    hsvToRgb(hsv, _red, _green, _blue);
}

//////////////////////////////////////////////////////////////////////////
static void applyTint(CGFloat * const _red, CGFloat * const _green, CGFloat * const _blue, CGFloat _tint)
{
    if(std::fabs(_tint) <= 0.0001)
    {
        return;
    }

    const CGFloat amount = std::min<CGFloat>(1.0, std::fabs(_tint)) * 0.18;
    const CGFloat targetRed = _tint >= 0.0 ? 1.0 : 0.0;
    const CGFloat targetGreen = _tint >= 0.0 ? 0.0 : 1.0;
    const CGFloat targetBlue = _tint >= 0.0 ? 1.0 : 0.0;

    *_red = clampUnit(*_red * (1.0 - amount) + targetRed * amount);
    *_green = clampUnit(*_green * (1.0 - amount) + targetGreen * amount);
    *_blue = clampUnit(*_blue * (1.0 - amount) + targetBlue * amount);
}

//////////////////////////////////////////////////////////////////////////
static CGFloat commandImageFilterValue(const ViewerRenderCommand & _command, std::size_t _filterColorAdjustIndex, std::size_t _paintFilterIndex)
{
    CGFloat value = 0.0;
    if(_command.hasFilterColorAdjustValue == true && _filterColorAdjustIndex < std::size(_command.filterColorAdjust))
    {
        value += static_cast<CGFloat>(_command.filterColorAdjust[_filterColorAdjustIndex]);
    }

    if(_command.hasPaintFilterValue == true && _paintFilterIndex < std::size(_command.paintFilter))
    {
        value += static_cast<CGFloat>(_command.paintFilter[_paintFilterIndex]);
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static bool commandHasImageFilter(const ViewerRenderCommand & _command)
{
    constexpr std::size_t SupportedSharedFilterIndices[] = {0, 1, 2, 4, 6, 7};
    for(std::size_t index : SupportedSharedFilterIndices)
    {
        if(std::fabs(commandImageFilterValue(_command, index, index)) > 0.0001)
        {
            return true;
        }
    }

    if(_command.hasPaintFilterValue == true)
    {
        return std::fabs(_command.paintFilter[8]) > 0.0001f || std::fabs(_command.paintFilter[9]) > 0.0001f;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
static NSImage * imageByApplyingFilter(NSImage * _image, const ViewerRenderCommand & _command)
{
    if(commandHasImageFilter(_command) == false)
    {
        return _image;
    }

    CGImageRef sourceImage = [_image CGImageForProposedRect:nullptr context:nil hints:nil];
    if(sourceImage == nullptr)
    {
        return _image;
    }

    const std::size_t width = CGImageGetWidth(sourceImage);
    const std::size_t height = CGImageGetHeight(sourceImage);
    if(width == 0 || height == 0)
    {
        return _image;
    }

    std::vector<std::uint8_t> pixels(width * height * 4);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels.data(), width, height, 8, width * 4, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    if(context == nullptr)
    {
        return _image;
    }

    CGContextDrawImage(context, CGRectMake(0.0, 0.0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)), sourceImage);

    const CGFloat tint = commandImageFilterValue(_command, 0, 0);
    const CGFloat shadows = commandImageFilterValue(_command, 1, 1);
    const CGFloat highlights = commandImageFilterValue(_command, 2, 2);
    const CGFloat exposure = commandImageFilterValue(_command, 4, 4);
    const CGFloat temperature = commandImageFilterValue(_command, 6, 6);
    const CGFloat vibrance = commandImageFilterValue(_command, 7, 7);
    const CGFloat contrast = _command.hasPaintFilterValue == true ? static_cast<CGFloat>(_command.paintFilter[8]) : 0.0;
    const CGFloat brightness = _command.hasPaintFilterValue == true ? static_cast<CGFloat>(_command.paintFilter[9]) : 0.0;

    const std::size_t pixelSize = pixels.size();
    for(std::size_t index = 0; index != pixelSize; index += 4)
    {
        const CGFloat alpha = static_cast<CGFloat>(pixels[index + 3]) / 255.0;
        if(alpha <= 0.0)
        {
            continue;
        }

        CGFloat red = static_cast<CGFloat>(pixels[index + 0]) / 255.0 / alpha;
        CGFloat green = static_cast<CGFloat>(pixels[index + 1]) / 255.0 / alpha;
        CGFloat blue = static_cast<CGFloat>(pixels[index + 2]) / 255.0 / alpha;

        applyBrightness(&red, &green, &blue, brightness);
        applyExposure(&red, &green, &blue, exposure);
        applyShadowsHighlights(&red, &green, &blue, shadows, highlights);
        applyContrast(&red, &green, &blue, contrast);
        applyTemperature(&red, &green, &blue, temperature);
        applyTint(&red, &green, &blue, tint);
        applyVibrance(&red, &green, &blue, vibrance);

        pixels[index + 0] = static_cast<std::uint8_t>(std::lround(clampUnit(red) * alpha * 255.0));
        pixels[index + 1] = static_cast<std::uint8_t>(std::lround(clampUnit(green) * alpha * 255.0));
        pixels[index + 2] = static_cast<std::uint8_t>(std::lround(clampUnit(blue) * alpha * 255.0));
    }

    CGImageRef filteredImage = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    if(filteredImage == nullptr)
    {
        return _image;
    }

    NSImage * result = [[NSImage alloc] initWithCGImage:filteredImage size:_image.size];
    CGImageRelease(filteredImage);

    return result ?: _image;
}

//////////////////////////////////////////////////////////////////////////
std::string resolveFigPath(const char * _requestedPath)
{
    std::filesystem::path path(_requestedPath);
    if(std::filesystem::exists(path) == true)
    {
        return path.string();
    }

    std::filesystem::path zipPath(path.string() + ".zip");
    if(std::filesystem::exists(zipPath) == true)
    {
        return zipPath.string();
    }

    return path.string();
}

//////////////////////////////////////////////////////////////////////////
static bool readFileBytes(const char * _path, std::vector<std::uint8_t> * const _bytes)
{
    if(_path == nullptr || _bytes == nullptr)
    {
        return false;
    }

    std::ifstream stream(_path, std::ios::binary);
    if(stream.good() == false)
    {
        return false;
    }

    stream.seekg(0, std::ios::end);
    const std::streampos size = stream.tellg();
    if(size < 0)
    {
        return false;
    }

    stream.seekg(0, std::ios::beg);
    _bytes->resize(static_cast<std::size_t>(size));
    if(_bytes->empty() == true)
    {
        return true;
    }

    stream.read(reinterpret_cast<char *>(_bytes->data()), static_cast<std::streamsize>(_bytes->size()));

    return stream.good();
}

//////////////////////////////////////////////////////////////////////////
static bool readFileText(const char * _path, std::string * const _text)
{
    if(_text == nullptr)
    {
        return false;
    }

    std::vector<std::uint8_t> bytes;
    if(readFileBytes(_path, &bytes) == false)
    {
        return false;
    }

    _text->assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());

    return true;
}

//////////////////////////////////////////////////////////////////////////
float prototypeIntroAdvanceTime(const figma_document_t * _document)
{
    return figma_inspection_get_prototype_intro_advance_time(_document);
}

//////////////////////////////////////////////////////////////////////////
figma_player_desc_t makePlayerDesc(const figma_document_t * _document)
{
    figma_player_desc_t playerDesc = {};
    if(_document != nullptr)
    {
        figma_rectf_t rect = {};
        if(figma_document_get_prototype_start_frame_rect(
               _document, &rect) != FIGMA_FALSE)
        {
            playerDesc.viewport.width = std::max(1.0f, rect.w);
            playerDesc.viewport.height = std::max(1.0f, rect.h);
            playerDesc.viewport.scale = 1.0f;
            return playerDesc;
        }
    }

    playerDesc.viewport.width = 428.0f;
    playerDesc.viewport.height = 926.0f;
    playerDesc.viewport.scale = 1.0f;
    return playerDesc;
}

figma_result_t loadViewerDocument(figma_runtime_t * const _runtime,
                                         const std::string & _figPath,
                                         const char * _sidecarPath,
                                         figma_document_t ** const _document,
                                         figma_player_t ** const _player,
                                         figma_player_desc_t * const _playerDesc)
{
    if(_runtime == nullptr || _document == nullptr || _player == nullptr || _playerDesc == nullptr)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    *_document = nullptr;
    *_player = nullptr;

    std::vector<std::uint8_t> figBytes;
    if(readFileBytes(_figPath.c_str(), &figBytes) == false)
    {
        return FIGMA_RESULT_IO_FAILED;
    }

    figma_load_options_t loadOptions = {};
    loadOptions.source_name = {
        _figPath.data(), _figPath.size()};
    loadOptions.extract_image_assets = FIGMA_TRUE;
    loadOptions.keep_canvas_bytes = FIGMA_TRUE;

    figma_document_t * documentPtr = nullptr;
    figma_result_t result = figma_runtime_load_document_from_fig_data(
        _runtime,
        figBytes.data(),
        figBytes.size(),
        &loadOptions,
        &documentPtr);
    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }

    if(_sidecarPath != nullptr)
    {
        std::string uxData;
        if(readFileText(_sidecarPath, &uxData) == true)
        {
            (void)figma_document_load_ux(
                documentPtr, {uxData.data(), uxData.size()});
        }
    }

    figma_player_desc_t playerDesc = makePlayerDesc(documentPtr);
    figma_player_t * playerPtr = nullptr;
    result = figma_runtime_create_player(
        _runtime, documentPtr, &playerDesc, &playerPtr);
    if(result != FIGMA_RESULT_OK)
    {
        figma_document_destroy(documentPtr);
        return result;
    }

    const float initialAdvanceTime = prototypeIntroAdvanceTime(documentPtr);
    if(initialAdvanceTime > 0.0f)
    {
        (void)figma_player_update(playerPtr, initialAdvanceTime);
    }

    *_document = documentPtr;
    *_player = playerPtr;
    *_playerDesc = playerDesc;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
NSInteger playbackSpeedValueCount()
{
    static const CGFloat values[] = {0.01, 0.1, 0.2, 1.0 / 3.0, 0.5, 1.0};
    return static_cast<NSInteger>(sizeof(values) / sizeof(values[0]));
}

//////////////////////////////////////////////////////////////////////////
CGFloat playbackSpeedAtIndex(NSInteger _index)
{
    static const CGFloat values[] = {0.01, 0.1, 0.2, 1.0 / 3.0, 0.5, 1.0};
    const NSInteger index = std::max<NSInteger>(0, std::min<NSInteger>(_index, playbackSpeedValueCount() - 1));
    return values[index];
}

//////////////////////////////////////////////////////////////////////////
NSInteger defaultPlaybackSpeedIndex()
{
    return playbackSpeedValueCount() - 1;
}

//////////////////////////////////////////////////////////////////////////
NSString * playbackSpeedLabel(CGFloat _speed)
{
    if(_speed < 1.0)
    {
        const CGFloat divider = std::round(1.0 / std::max<CGFloat>(0.0001, _speed));
        return [NSString stringWithFormat:@"1/%.0fx", divider];
    }

    return [NSString stringWithFormat:@"%.0fx", _speed];
}

//////////////////////////////////////////////////////////////////////////
figma_pointer_button_t pointerButtonFromEvent(NSEvent * _event)
{
    switch(_event.buttonNumber)
    {
    case 0:
        return FIGMA_POINTER_BUTTON_LEFT;
    case 1:
        return FIGMA_POINTER_BUTTON_RIGHT;
    case 2:
        return FIGMA_POINTER_BUTTON_MIDDLE;
    default:
        return FIGMA_POINTER_BUTTON_OTHER;
    }
}

//////////////////////////////////////////////////////////////////////////
figma_input_modifier_flags_t inputModifiersFromEvent(NSEvent * _event)
{
    figma_input_modifier_flags_t modifiers = FIGMA_INPUT_MODIFIER_NONE;
    const NSEventModifierFlags flags = _event.modifierFlags;
    if((flags & NSEventModifierFlagShift) != 0)
    {
        modifiers |= FIGMA_INPUT_MODIFIER_SHIFT;
    }
    if((flags & NSEventModifierFlagControl) != 0)
    {
        modifiers |= FIGMA_INPUT_MODIFIER_CONTROL;
    }
    if((flags & NSEventModifierFlagOption) != 0)
    {
        modifiers |= FIGMA_INPUT_MODIFIER_ALT;
    }
    if((flags & NSEventModifierFlagCommand) != 0)
    {
        modifiers |= FIGMA_INPUT_MODIFIER_COMMAND;
    }

    return modifiers;
}

//////////////////////////////////////////////////////////////////////////
CGRect cgRectFromNSRect(NSRect _rect)
{
    return CGRectMake(_rect.origin.x, _rect.origin.y, _rect.size.width, _rect.size.height);
}

//////////////////////////////////////////////////////////////////////////
void addShapePath(CGContextRef _context, figma_render_shape_type_t _shape, NSRect _rect, CGFloat _radius)
{
    if(_shape == FIGMA_RENDER_SHAPE_ELLIPSE)
    {
        CGContextAddEllipseInRect(_context, cgRectFromNSRect(_rect));
        return;
    }

    if(_shape == FIGMA_RENDER_SHAPE_ROUNDED_RECTANGLE && _radius > 0.0)
    {
        CGPathRef path = CGPathCreateWithRoundedRect(cgRectFromNSRect(_rect), _radius, _radius, nullptr);
        CGContextAddPath(_context, path);
        CGPathRelease(path);
        return;
    }

    CGContextAddRect(_context, cgRectFromNSRect(_rect));
}

//////////////////////////////////////////////////////////////////////////
static CGFloat averageImageUAtX(const ViewerRenderCommand & _command, CGFloat _x)
{
    CGFloat sum = 0.0;
    CGFloat count = 0.0;
    const CGFloat epsilon = std::max<CGFloat>(0.01, _command.rect.w * 0.01);
    for(const figma_render_vertex_t & vertex : _command.vertices)
    {
        if(std::fabs(static_cast<CGFloat>(vertex.x) - _x) <= epsilon)
        {
            sum += static_cast<CGFloat>(vertex.u);
            count += 1.0;
        }
    }

    return count > 0.0 ? sum / count : 0.0;
}

//////////////////////////////////////////////////////////////////////////
static CGFloat averageImageVAtY(const ViewerRenderCommand & _command, CGFloat _y)
{
    CGFloat sum = 0.0;
    CGFloat count = 0.0;
    const CGFloat epsilon = std::max<CGFloat>(0.01, _command.rect.h * 0.01);
    for(const figma_render_vertex_t & vertex : _command.vertices)
    {
        if(std::fabs(static_cast<CGFloat>(vertex.y) - _y) <= epsilon)
        {
            sum += static_cast<CGFloat>(vertex.v);
            count += 1.0;
        }
    }

    return count > 0.0 ? sum / count : 0.0;
}

//////////////////////////////////////////////////////////////////////////
static NSRect sourceRectFromImageUvs(CGFloat _u0, CGFloat _v0, CGFloat _u1, CGFloat _v1, NSSize _imageSize)
{
    const CGFloat u0 = std::clamp<CGFloat>(std::min(_u0, _u1), 0.0, 1.0);
    const CGFloat v0 = std::clamp<CGFloat>(std::min(_v0, _v1), 0.0, 1.0);
    const CGFloat u1 = std::clamp<CGFloat>(std::max(_u0, _u1), 0.0, 1.0);
    const CGFloat v1 = std::clamp<CGFloat>(std::max(_v0, _v1), 0.0, 1.0);
    const CGFloat width = std::max<CGFloat>(1.0, _imageSize.width);
    const CGFloat height = std::max<CGFloat>(1.0, _imageSize.height);

    return NSMakeRect(u0 * width, v0 * height, std::max<CGFloat>(1.0, (u1 - u0) * width), std::max<CGFloat>(1.0, (v1 - v0) * height));
}

//////////////////////////////////////////////////////////////////////////
static NSSize targetPixelSizeForRect(CGContextRef _context, NSRect _rect)
{
    CGSize size = CGSizeMake(std::max<CGFloat>(1.0, _rect.size.width), std::max<CGFloat>(1.0, _rect.size.height));
    if(_context != nullptr)
    {
        size = CGContextConvertSizeToDeviceSpace(_context, size);
    }

    return NSMakeSize(std::max<CGFloat>(1.0, std::ceil(std::fabs(size.width))), std::max<CGFloat>(1.0, std::ceil(std::fabs(size.height))));
}

//////////////////////////////////////////////////////////////////////////
static CGFloat pixelScaleForCurrentContext()
{
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    if(context == nullptr)
    {
        return 1.0;
    }

    const CGSize size = CGContextConvertSizeToDeviceSpace(context, CGSizeMake(1.0, 1.0));
    return std::max<CGFloat>(1.0, std::max<CGFloat>(std::fabs(size.width), std::fabs(size.height)));
}

//////////////////////////////////////////////////////////////////////////
static bool shouldDownsampleImage(NSRect _sourceRect, NSSize _targetPixelSize)
{
    return _sourceRect.size.width > _targetPixelSize.width * 1.15 || _sourceRect.size.height > _targetPixelSize.height * 1.15;
}

//////////////////////////////////////////////////////////////////////////
static NSImage * downsampledImageForDraw(NSImage * _image, NSRect _sourceRect, NSSize _targetPointSize, NSSize _targetPixelSize)
{
    const CGFloat targetWidth = std::max<CGFloat>(1.0, _targetPixelSize.width);
    const CGFloat targetHeight = std::max<CGFloat>(1.0, _targetPixelSize.height);
    NSImage * result = [[NSImage alloc] initWithSize:NSMakeSize(targetWidth, targetHeight)];
    if(result == nil)
    {
        return nil;
    }

    [result lockFocusFlipped:YES];
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
    [[NSColor clearColor] setFill];
    NSRectFillUsingOperation(NSMakeRect(0.0, 0.0, targetWidth, targetHeight), NSCompositingOperationCopy);
    [_image drawInRect:NSMakeRect(0.0, 0.0, targetWidth, targetHeight)
              fromRect:_sourceRect
             operation:NSCompositingOperationSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
    [result unlockFocus];

    result.size = NSMakeSize(std::max<CGFloat>(1.0, _targetPointSize.width), std::max<CGFloat>(1.0, _targetPointSize.height));
    return result;
}

//////////////////////////////////////////////////////////////////////////
void drawImageCommand(NSImage * _image, const figma_asset_desc_t * _asset, const ViewerRenderCommand & _command, NSRect _rect)
{
    if(_image == nil)
    {
        return;
    }

    NSImage * image = imageByApplyingFilter(_image, _command);

    (void)_asset;
    CGFloat u0 = 0.0;
    CGFloat v0 = 0.0;
    CGFloat u1 = 1.0;
    CGFloat v1 = 1.0;
    bool flipX = false;
    bool flipY = false;
    if(_command.vertices.empty() == false)
    {
        u0 = u1 = static_cast<CGFloat>(_command.vertices.front().u);
        v0 = v1 = static_cast<CGFloat>(_command.vertices.front().v);
        CGFloat minX = static_cast<CGFloat>(_command.vertices.front().x);
        CGFloat maxX = minX;
        CGFloat minY = static_cast<CGFloat>(_command.vertices.front().y);
        CGFloat maxY = minY;
        for(const figma_render_vertex_t & vertex : _command.vertices)
        {
            u0 = std::min<CGFloat>(u0, static_cast<CGFloat>(vertex.u));
            v0 = std::min<CGFloat>(v0, static_cast<CGFloat>(vertex.v));
            u1 = std::max<CGFloat>(u1, static_cast<CGFloat>(vertex.u));
            v1 = std::max<CGFloat>(v1, static_cast<CGFloat>(vertex.v));
            minX = std::min<CGFloat>(minX, static_cast<CGFloat>(vertex.x));
            maxX = std::max<CGFloat>(maxX, static_cast<CGFloat>(vertex.x));
            minY = std::min<CGFloat>(minY, static_cast<CGFloat>(vertex.y));
            maxY = std::max<CGFloat>(maxY, static_cast<CGFloat>(vertex.y));
        }

        if(std::fabs(maxX - minX) > 0.001)
        {
            flipX = averageImageUAtX(_command, minX) > averageImageUAtX(_command, maxX);
        }

        if(std::fabs(maxY - minY) > 0.001)
        {
            flipY = averageImageVAtY(_command, minY) > averageImageVAtY(_command, maxY);
        }
    }

    const NSRect sourceRect = sourceRectFromImageUvs(u0, v0, u1, v1, image.size);

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, cgBlendModeForCommand(_command));
    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
    CGContextBeginPath(context);
    addShapePath(context, _command.shape, _rect, static_cast<CGFloat>(_command.cornerRadius));
    CGContextClip(context);

    NSRect drawSourceRect = sourceRect;
    NSImage * drawImage = image;
    const NSSize targetPixelSize = targetPixelSizeForRect(context, _rect);
    if(shouldDownsampleImage(sourceRect, targetPixelSize) == true)
    {
        NSImage * downsampledImage = downsampledImageForDraw(image, sourceRect, _rect.size, targetPixelSize);
        if(downsampledImage != nil)
        {
            drawImage = downsampledImage;
            drawSourceRect = NSMakeRect(0.0, 0.0, drawImage.size.width, drawImage.size.height);
        }
    }

    if(flipX == true || flipY == true)
    {
        NSAffineTransform * transform = [NSAffineTransform transform];
        [transform translateXBy:NSMidX(_rect) yBy:NSMidY(_rect)];
        [transform scaleXBy:flipX == true ? -1.0 : 1.0 yBy:flipY == true ? -1.0 : 1.0];
        [transform translateXBy:-NSMidX(_rect) yBy:-NSMidY(_rect)];
        [transform concat];
    }

    [drawImage drawInRect:_rect fromRect:drawSourceRect operation:compositingOperationForCommand(_command) fraction:_command.opacity respectFlipped:YES hints:nil];
    CGContextRestoreGState(context);
}

//////////////////////////////////////////////////////////////////////////
void drawMeshCommand(const ViewerRenderCommand & _command)
{
    if(_command.vertices.empty() == true || _command.indices.size() < 3)
    {
        return;
    }

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    if(context == nullptr)
    {
        return;
    }

    CGContextSaveGState(context);
    CGContextSetBlendMode(context, cgBlendModeForCommand(_command));
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetShouldAntialias(context, true);
    [colorFromVertex(_command.vertices[_command.indices.front()]) setFill];
    CGContextBeginPath(context);
    const std::size_t indexSize = _command.indices.size();
    const std::size_t vertexSize = _command.vertices.size();
    for(std::size_t index = 0; index + 2 < indexSize; index += 3)
    {
        const std::uint16_t i0 = _command.indices[index + 0];
        const std::uint16_t i1 = _command.indices[index + 1];
        const std::uint16_t i2 = _command.indices[index + 2];
        if(i0 >= vertexSize || i1 >= vertexSize || i2 >= vertexSize)
        {
            continue;
        }

        const figma_render_vertex_t & v0 = _command.vertices[i0];
        const figma_render_vertex_t & v1 = _command.vertices[i1];
        const figma_render_vertex_t & v2 = _command.vertices[i2];

        CGContextMoveToPoint(context, static_cast<CGFloat>(v0.x), static_cast<CGFloat>(v0.y));
        CGContextAddLineToPoint(context, static_cast<CGFloat>(v1.x), static_cast<CGFloat>(v1.y));
        CGContextAddLineToPoint(context, static_cast<CGFloat>(v2.x), static_cast<CGFloat>(v2.y));
        CGContextClosePath(context);
    }
    CGContextFillPath(context);
    CGContextRestoreGState(context);
}
