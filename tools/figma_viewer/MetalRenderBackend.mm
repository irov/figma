#include "MetalRenderBackend.h"

#include "../../sdk/src/RenderList.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <setjmp.h>
#include <sstream>
#include <utility>

#include "png.h"

extern "C"
{
#include "jpeglib.h"
}

namespace
{
    //////////////////////////////////////////////////////////////////////////
    static std::string stdString(const Figma::FigmaString & _value)
    {
        return std::string(_value.data(), _value.size());
    }

    //////////////////////////////////////////////////////////////////////////
    static const Figma::RenderCommandVector & privateRenderCommands(const Figma::RenderListInterface * const _renderList)
    {
        const Figma::RenderList & renderList = static_cast<const Figma::RenderList &>(*_renderList);
        return renderList.getCommands();
    }
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
static CGFloat commandImageFilterValue(const Figma::RenderCommand & _command, std::size_t _filterColorAdjustIndex, std::size_t _paintFilterIndex)
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
static bool commandHasImageFilter(const Figma::RenderCommand & _command)
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

struct DecodedImagePixelsDesc
{
    std::vector<std::uint8_t> pixels;
    NSUInteger width = 0;
    NSUInteger height = 0;
};

struct JpegErrorDesc
{
    jpeg_error_mgr error;
    jmp_buf jumpBuffer;
};

//////////////////////////////////////////////////////////////////////////
static void jpegErrorExit(j_common_ptr _info)
{
    JpegErrorDesc * error = reinterpret_cast<JpegErrorDesc *>(_info->err);
    longjmp(error->jumpBuffer, 1);
}

//////////////////////////////////////////////////////////////////////////
static bool decodePngPixels(const std::uint8_t * _bytes, std::size_t _size, DecodedImagePixelsDesc * const _image)
{
    png_image image = {};
    image.version = PNG_IMAGE_VERSION;

    if(png_image_begin_read_from_memory(&image, _bytes, _size) == 0)
    {
        return false;
    }

    image.format = PNG_FORMAT_RGBA;
    std::vector<std::uint8_t> pixels(PNG_IMAGE_SIZE(image));
    if(png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr) == 0)
    {
        png_image_free(&image);
        return false;
    }

    _image->width = static_cast<NSUInteger>(image.width);
    _image->height = static_cast<NSUInteger>(image.height);
    _image->pixels = std::move(pixels);
    png_image_free(&image);

    return _image->width != 0 && _image->height != 0 && _image->pixels.empty() == false;
}

//////////////////////////////////////////////////////////////////////////
static bool decodeJpegPixels(const std::uint8_t * _bytes, std::size_t _size, DecodedImagePixelsDesc * const _image)
{
    jpeg_decompress_struct info = {};
    JpegErrorDesc error = {};
    info.err = jpeg_std_error(&error.error);
    error.error.error_exit = jpegErrorExit;

    if(setjmp(error.jumpBuffer) != 0)
    {
        jpeg_destroy_decompress(&info);
        return false;
    }

    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, const_cast<unsigned char *>(_bytes), static_cast<unsigned long>(_size));
    jpeg_read_header(&info, TRUE);
    info.out_color_space = JCS_RGB;
    jpeg_start_decompress(&info);

    const NSUInteger width = static_cast<NSUInteger>(info.output_width);
    const NSUInteger height = static_cast<NSUInteger>(info.output_height);
    const int components = static_cast<int>(info.output_components);
    if(width == 0 || height == 0 || components < 3)
    {
        jpeg_finish_decompress(&info);
        jpeg_destroy_decompress(&info);
        return false;
    }

    std::vector<std::uint8_t> pixels(width * height * 4);
    std::vector<JSAMPLE> row(width * static_cast<NSUInteger>(components));
    while(info.output_scanline < info.output_height)
    {
        JSAMPROW rowPointer = row.data();
        const JDIMENSION y = info.output_scanline;
        jpeg_read_scanlines(&info, &rowPointer, 1);

        std::uint8_t * target = pixels.data() + static_cast<NSUInteger>(y) * width * 4;
        for(NSUInteger x = 0; x != width; ++x)
        {
            const JSAMPLE * source = row.data() + x * static_cast<NSUInteger>(components);
            target[x * 4 + 0] = source[0];
            target[x * 4 + 1] = source[1];
            target[x * 4 + 2] = source[2];
            target[x * 4 + 3] = 255;
        }
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);

    _image->width = width;
    _image->height = height;
    _image->pixels = std::move(pixels);

    return true;
}

//////////////////////////////////////////////////////////////////////////
static bool decodeAssetPixels(const Figma::AssetDesc & _asset, DecodedImagePixelsDesc * const _image)
{
    if(_asset.bytes.empty() == true)
    {
        return false;
    }

    const std::uint8_t * bytes = _asset.bytes.data();
    const std::size_t size = _asset.bytes.size();
    if(size >= 8 && std::memcmp(bytes, "\x89PNG\r\n\x1a\n", 8) == 0)
    {
        return decodePngPixels(bytes, size, _image);
    }

    if(size >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff)
    {
        return decodeJpegPixels(bytes, size, _image);
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
static void applyImageFilterToPixels(DecodedImagePixelsDesc * const _image, const Figma::RenderCommand & _command)
{
    if(_image == nullptr || commandHasImageFilter(_command) == false)
    {
        return;
    }

    const CGFloat tint = commandImageFilterValue(_command, 0, 0);
    const CGFloat shadows = commandImageFilterValue(_command, 1, 1);
    const CGFloat highlights = commandImageFilterValue(_command, 2, 2);
    const CGFloat exposure = commandImageFilterValue(_command, 4, 4);
    const CGFloat temperature = commandImageFilterValue(_command, 6, 6);
    const CGFloat vibrance = commandImageFilterValue(_command, 7, 7);
    const CGFloat contrast = _command.hasPaintFilterValue == true ? static_cast<CGFloat>(_command.paintFilter[8]) : 0.0;
    const CGFloat brightness = _command.hasPaintFilterValue == true ? static_cast<CGFloat>(_command.paintFilter[9]) : 0.0;

    const std::size_t pixelSize = _image->pixels.size();
    for(std::size_t index = 0; index != pixelSize; index += 4)
    {
        std::uint8_t * pixel = _image->pixels.data() + index;
        const CGFloat alpha = static_cast<CGFloat>(pixel[3]) / 255.0;
        if(alpha <= 0.0)
        {
            continue;
        }

        CGFloat red = static_cast<CGFloat>(pixel[0]) / 255.0;
        CGFloat green = static_cast<CGFloat>(pixel[1]) / 255.0;
        CGFloat blue = static_cast<CGFloat>(pixel[2]) / 255.0;

        applyBrightness(&red, &green, &blue, brightness);
        applyExposure(&red, &green, &blue, exposure);
        applyShadowsHighlights(&red, &green, &blue, shadows, highlights);
        applyContrast(&red, &green, &blue, contrast);
        applyTemperature(&red, &green, &blue, temperature);
        applyTint(&red, &green, &blue, tint);
        applyVibrance(&red, &green, &blue, vibrance);

        pixel[0] = static_cast<std::uint8_t>(std::lround(clampUnit(red) * 255.0));
        pixel[1] = static_cast<std::uint8_t>(std::lround(clampUnit(green) * 255.0));
        pixel[2] = static_cast<std::uint8_t>(std::lround(clampUnit(blue) * 255.0));
    }
}


enum class EMetalBlendMode : std::uint32_t
{
    Normal = 0,
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
    Exclusion
};

enum class EMetalShapeType : std::uint32_t
{
    Rectangle = 0,
    RoundedRectangle,
    Ellipse
};




//////////////////////////////////////////////////////////////////////////
static NSString * metalTextureCacheKey(const Figma::RenderCommand & _command)
{
    std::ostringstream stream;
    stream << stdString(_command.assetId);
    stream << "|fca:" << _command.hasFilterColorAdjustValue;
    for(float value : _command.filterColorAdjust)
    {
        stream << ':' << value;
    }
    stream << "|pf:" << _command.hasPaintFilterValue;
    for(float value : _command.paintFilter)
    {
        stream << ':' << value;
    }

    const std::string key = stream.str();
    return [[NSString alloc] initWithBytes:key.data() length:key.size() encoding:NSUTF8StringEncoding] ?: @"";
}

//////////////////////////////////////////////////////////////////////////
static EMetalBlendMode metalBlendModeForCommand(const Figma::RenderCommand & _command)
{
    switch(_command.blendMode)
    {
    case Figma::ERenderBlendMode::Multiply:
        return EMetalBlendMode::Multiply;
    case Figma::ERenderBlendMode::Screen:
        return EMetalBlendMode::Screen;
    case Figma::ERenderBlendMode::Overlay:
        return EMetalBlendMode::Overlay;
    case Figma::ERenderBlendMode::Darken:
        return EMetalBlendMode::Darken;
    case Figma::ERenderBlendMode::Lighten:
        return EMetalBlendMode::Lighten;
    case Figma::ERenderBlendMode::ColorDodge:
        return EMetalBlendMode::ColorDodge;
    case Figma::ERenderBlendMode::ColorBurn:
        return EMetalBlendMode::ColorBurn;
    case Figma::ERenderBlendMode::SoftLight:
        return EMetalBlendMode::SoftLight;
    case Figma::ERenderBlendMode::HardLight:
        return EMetalBlendMode::HardLight;
    case Figma::ERenderBlendMode::Difference:
        return EMetalBlendMode::Difference;
    case Figma::ERenderBlendMode::Exclusion:
        return EMetalBlendMode::Exclusion;
    case Figma::ERenderBlendMode::PassThrough:
    case Figma::ERenderBlendMode::Normal:
    case Figma::ERenderBlendMode::Hue:
    case Figma::ERenderBlendMode::Saturation:
    case Figma::ERenderBlendMode::Color:
    case Figma::ERenderBlendMode::Luminosity:
    case Figma::ERenderBlendMode::Unsupported:
        break;
    }

    return EMetalBlendMode::Normal;
}

//////////////////////////////////////////////////////////////////////////
static EMetalShapeType metalShapeForCommand(const Figma::RenderCommand & _command)
{
    switch(_command.shape)
    {
    case Figma::ERenderShapeType::RoundedRectangle:
        return _command.cornerRadius > 0.0f ? EMetalShapeType::RoundedRectangle : EMetalShapeType::Rectangle;
    case Figma::ERenderShapeType::Ellipse:
        return EMetalShapeType::Ellipse;
    case Figma::ERenderShapeType::Rectangle:
        break;
    }

    return EMetalShapeType::Rectangle;
}

//////////////////////////////////////////////////////////////////////////
static bool metalBlendModeIsSupported(const Figma::RenderCommand & _command)
{
    switch(_command.blendMode)
    {
    case Figma::ERenderBlendMode::PassThrough:
    case Figma::ERenderBlendMode::Normal:
    case Figma::ERenderBlendMode::Multiply:
    case Figma::ERenderBlendMode::Screen:
    case Figma::ERenderBlendMode::Overlay:
    case Figma::ERenderBlendMode::Darken:
    case Figma::ERenderBlendMode::Lighten:
    case Figma::ERenderBlendMode::ColorDodge:
    case Figma::ERenderBlendMode::ColorBurn:
    case Figma::ERenderBlendMode::SoftLight:
    case Figma::ERenderBlendMode::HardLight:
    case Figma::ERenderBlendMode::Difference:
    case Figma::ERenderBlendMode::Exclusion:
        return true;
    case Figma::ERenderBlendMode::Hue:
    case Figma::ERenderBlendMode::Saturation:
    case Figma::ERenderBlendMode::Color:
    case Figma::ERenderBlendMode::Luminosity:
    case Figma::ERenderBlendMode::Unsupported:
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
static const char * metalShaderSource()
{
    return R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
    float2 position;
    float2 uv;
    float4 color;
};

struct Uniforms
{
    float2 viewportSize;
    float2 targetSize;
    float4 commandRect;
    float opacity;
    uint hasTexture;
    uint shape;
    uint blendMode;
    uint4 pad0;
    float4 pad1;
};

struct VertexOut
{
    float4 position [[position]];
    float2 uv;
    float4 color;
    float2 viewportPosition;
};

static float3 blend_overlay(float3 dst, float3 src)
{
    return select(1.0 - 2.0 * (1.0 - dst) * (1.0 - src), 2.0 * dst * src, dst <= 0.5);
}

static float3 blend_soft_light(float3 dst, float3 src)
{
    float3 dark = dst - (1.0 - 2.0 * src) * dst * (1.0 - dst);
    float3 light = dst + (2.0 * src - 1.0) * (sqrt(max(dst, 0.0)) - dst);
    return select(light, dark, src <= 0.5);
}

static float3 blend_rgb(float3 dst, float3 src, uint mode)
{
    switch(mode)
    {
    case 1:
        return dst * src;
    case 2:
        return 1.0 - (1.0 - dst) * (1.0 - src);
    case 3:
        return blend_overlay(dst, src);
    case 4:
        return min(dst, src);
    case 5:
        return max(dst, src);
    case 6:
    {
        float3 dodge = min(1.0, dst / max(1.0 - src, float3(0.00001)));
        return select(dodge, float3(1.0), src >= 1.0);
    }
    case 7:
    {
        float3 burn = 1.0 - min(1.0, (1.0 - dst) / max(src, float3(0.00001)));
        return select(burn, float3(0.0), src <= 0.0);
    }
    case 8:
        return blend_soft_light(dst, src);
    case 9:
        return blend_overlay(src, dst);
    case 10:
        return abs(dst - src);
    case 11:
        return dst + src - 2.0 * dst * src;
    default:
        return src;
    }
}

static bool outside_shape(float2 point, Uniforms uniforms)
{
    if(uniforms.shape == 0)
    {
        return false;
    }

    float2 rectMin = uniforms.commandRect.xy;
    float2 rectSize = max(uniforms.commandRect.zw, float2(0.0001));
    float2 local = point - rectMin;
    if(any(local < 0.0) || any(local > rectSize))
    {
        return true;
    }

    if(uniforms.shape == 2)
    {
        float2 center = rectSize * 0.5;
        float2 normalized = (local - center) / max(center, float2(0.0001));
        return dot(normalized, normalized) > 1.0;
    }

    float radius = clamp(uniforms.pad1.x, 0.0, min(rectSize.x, rectSize.y) * 0.5);
    float2 halfSize = rectSize * 0.5;
    float2 q = abs(local - halfSize) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) > radius;
}

vertex VertexOut figma_vertex(uint vertexId [[vertex_id]],
                              const device VertexIn * vertices [[buffer(0)]],
                              constant Uniforms & uniforms [[buffer(1)]])
{
    VertexIn inVertex = vertices[vertexId];
    float2 viewportSize = max(uniforms.viewportSize, float2(1.0));
    float2 ndc = float2(inVertex.position.x / viewportSize.x * 2.0 - 1.0,
                       1.0 - inVertex.position.y / viewportSize.y * 2.0);

    VertexOut outVertex;
    outVertex.position = float4(ndc, 0.0, 1.0);
    outVertex.uv = inVertex.uv;
    outVertex.color = inVertex.color;
    outVertex.viewportPosition = inVertex.position;
    return outVertex;
}

fragment float4 figma_fragment(VertexOut inVertex [[stage_in]],
                               texture2d<float> sourceTexture [[texture(0)]],
                               texture2d<float> backdropTexture [[texture(1)]],
                               sampler textureSampler [[sampler(0)]],
                               constant Uniforms & uniforms [[buffer(0)]])
{
    if(outside_shape(inVertex.viewportPosition, uniforms))
    {
        discard_fragment();
    }

    float4 dst = backdropTexture.sample(textureSampler, inVertex.position.xy / max(uniforms.targetSize, float2(1.0)));
    float4 src;
    if(uniforms.hasTexture != 0)
    {
        float4 texel = sourceTexture.sample(textureSampler, inVertex.uv);
        src = float4(texel.rgb * inVertex.color.rgb * inVertex.color.a, texel.a * inVertex.color.a);
    }
    else
    {
        src = float4(inVertex.color.rgb * inVertex.color.a, inVertex.color.a);
    }
    src.rgb *= uniforms.opacity;
    src.a *= uniforms.opacity;

    float3 dstStraight = dst.a > 0.00001 ? dst.rgb / dst.a : 0.0;
    float3 srcStraight = src.a > 0.00001 ? src.rgb / src.a : 0.0;
    float3 blended = blend_rgb(dstStraight, srcStraight, uniforms.blendMode);
    float3 sourceRgb = uniforms.blendMode == 0 ? src.rgb : blended * src.a;
    float outAlpha = src.a + dst.a * (1.0 - src.a);
    float3 outRgb = sourceRgb + dst.rgb * (1.0 - src.a);
    return float4(clamp(outRgb, 0.0, 1.0), clamp(outAlpha, 0.0, 1.0));
}

fragment float4 figma_copy_fragment(VertexOut inVertex [[stage_in]],
                                    texture2d<float> sourceTexture [[texture(0)]],
                                    sampler textureSampler [[sampler(0)]])
{
    float4 src = sourceTexture.sample(textureSampler, inVertex.uv);
    float3 rgb = src.a > 0.00001 ? src.rgb / src.a : 0.0;
    return float4(clamp(rgb, 0.0, 1.0), src.a);
}
)MSL";
}


//////////////////////////////////////////////////////////////////////////
MetalRenderBackend::MetalRenderBackend()
    : m_device(MTLCreateSystemDefaultDevice())
{
    if(m_device == nil)
    {
        return;
    }

    m_commandQueue = [m_device newCommandQueue];

    NSError * error = nil;
    NSString * source = [NSString stringWithUTF8String:metalShaderSource()];
    m_library = [m_device newLibraryWithSource:source options:nil error:&error];
    if(m_library == nil)
    {
        NSLog(@"Metal shader compile failed: %@", error);
        return;
    }

    m_commandPipeline = this->makePipeline(@"figma_vertex", @"figma_fragment", MTLPixelFormatBGRA8Unorm);
    m_copyPipeline = this->makePipeline(@"figma_vertex", @"figma_copy_fragment", MTLPixelFormatBGRA8Unorm);

    MTLSamplerDescriptor * samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    m_sampler = [m_device newSamplerStateWithDescriptor:samplerDesc];

    m_textureCache = [[NSMutableDictionary alloc] init];
    m_whiteTexture = this->makeWhiteTexture();
}

//////////////////////////////////////////////////////////////////////////
id<MTLDevice> MetalRenderBackend::device() const
{
    return m_device;
}

//////////////////////////////////////////////////////////////////////////
bool MetalRenderBackend::isValid() const
{
    return m_device != nil && m_commandQueue != nil && m_commandPipeline != nil && m_copyPipeline != nil && m_sampler != nil && m_whiteTexture != nil;
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::clearTextureCache()
{
    [m_textureCache removeAllObjects];
}

void MetalRenderBackend::render(CAMetalLayer * _layer,
            Figma::DocumentInterface * _document,
            FreeTypeTextRenderer * _textRenderer,
            const Figma::RenderListInterface * const _renderList,
            const std::vector<std::uint8_t> & _visibility,
            CGFloat _viewportWidth,
            CGFloat _viewportHeight,
            CGFloat _contentsScale)
{
    if(this->isValid() == false || _layer == nil)
    {
        return;
    }

    const CGSize boundsSize = _layer.bounds.size;
    const NSUInteger pixelWidth = static_cast<NSUInteger>(std::ceil(std::max<CGFloat>(1.0, boundsSize.width * _contentsScale)));
    const NSUInteger pixelHeight = static_cast<NSUInteger>(std::ceil(std::max<CGFloat>(1.0, boundsSize.height * _contentsScale)));
    if(pixelWidth == 0 || pixelHeight == 0)
    {
        return;
    }

    _layer.device = m_device;
    _layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    _layer.framebufferOnly = NO;
    _layer.contentsScale = _contentsScale;
    _layer.drawableSize = CGSizeMake(pixelWidth, pixelHeight);

    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    if(drawable == nil)
    {
        return;
    }

    id<MTLCommandBuffer> commandBuffer = [m_commandQueue commandBuffer];
    id<MTLTexture> currentTexture = this->makeRenderTexture(pixelWidth, pixelHeight);
    id<MTLTexture> backdropTexture = this->makeRenderTexture(pixelWidth, pixelHeight);
    if(commandBuffer == nil || currentTexture == nil || backdropTexture == nil)
    {
        return;
    }

    this->clearTexture(commandBuffer, currentTexture, MTLClearColorMake(0.0, 0.0, 0.0, 1.0));
    if(_renderList == nullptr)
    {
        return;
    }

    const Figma::RenderCommandVector & commands = privateRenderCommands(_renderList);
    this->renderCommandRange(commandBuffer,
                             _document,
                             _textRenderer,
                             commands,
                             0,
                             commands.size(),
                             _visibility,
                             currentTexture,
                             backdropTexture,
                             _viewportWidth,
                             _viewportHeight,
                             pixelWidth,
                             pixelHeight,
                             true);
    this->copyTexture(commandBuffer, currentTexture, drawable.texture, _viewportWidth, _viewportHeight, pixelWidth, pixelHeight);

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

//////////////////////////////////////////////////////////////////////////
id<MTLRenderPipelineState> MetalRenderBackend::makePipeline(NSString * _vertexName, NSString * _fragmentName, MTLPixelFormat _pixelFormat)
{
    MTLRenderPipelineDescriptor * desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = [m_library newFunctionWithName:_vertexName];
    desc.fragmentFunction = [m_library newFunctionWithName:_fragmentName];
    desc.colorAttachments[0].pixelFormat = _pixelFormat;
    desc.colorAttachments[0].blendingEnabled = NO;

    NSError * error = nil;
    id<MTLRenderPipelineState> pipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&error];
    if(pipeline == nil)
    {
        NSLog(@"Metal pipeline compile failed: %@", error);
    }

    return pipeline;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::makeWhiteTexture()
{
    MTLTextureDescriptor * desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> texture = [m_device newTextureWithDescriptor:desc];
    const std::uint8_t pixel[4] = {255, 255, 255, 255};
    [texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:pixel bytesPerRow:4];
    return texture;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::makeRenderTexture(NSUInteger _width, NSUInteger _height)
{
    MTLTextureDescriptor * desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:_width height:_height mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
    return [m_device newTextureWithDescriptor:desc];
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::clearTexture(id<MTLCommandBuffer> _commandBuffer, id<MTLTexture> _texture, MTLClearColor _color)
{
    MTLRenderPassDescriptor * pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = _texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = _color;
    id<MTLRenderCommandEncoder> encoder = [_commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder endEncoding];
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::copyTextureToTexture(id<MTLCommandBuffer> _commandBuffer, id<MTLTexture> _source, id<MTLTexture> _destination)
{
    id<MTLBlitCommandEncoder> blit = [_commandBuffer blitCommandEncoder];
    [blit copyFromTexture:_source
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(_source.width, _source.height, 1)
                toTexture:_destination
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
}

//////////////////////////////////////////////////////////////////////////
MetalRenderBackend::MipPixelsDesc MetalRenderBackend::makeNextMipLevel(const MetalRenderBackend::MipPixelsDesc & _source)
{
    MipPixelsDesc result;
    result.width = std::max<NSUInteger>(1, _source.width / 2);
    result.height = std::max<NSUInteger>(1, _source.height / 2);
    result.pixels.assign(result.width * result.height * 4, 0);

    for(NSUInteger y = 0; y != result.height; ++y)
    {
        for(NSUInteger x = 0; x != result.width; ++x)
        {
            const NSUInteger sourceX0 = x * 2;
            const NSUInteger sourceY0 = y * 2;
            const NSUInteger sourceX1 = std::min<NSUInteger>(_source.width, sourceX0 + 2);
            const NSUInteger sourceY1 = std::min<NSUInteger>(_source.height, sourceY0 + 2);

            std::uint32_t samples = 0;
            std::uint32_t redSum = 0;
            std::uint32_t greenSum = 0;
            std::uint32_t blueSum = 0;
            std::uint32_t alphaSum = 0;

            for(NSUInteger sourceY = sourceY0; sourceY != sourceY1; ++sourceY)
            {
                for(NSUInteger sourceX = sourceX0; sourceX != sourceX1; ++sourceX)
                {
                    const std::uint8_t * sourcePixel = _source.pixels.data() + (sourceY * _source.width + sourceX) * 4;
                    redSum += sourcePixel[0];
                    greenSum += sourcePixel[1];
                    blueSum += sourcePixel[2];
                    alphaSum += sourcePixel[3];
                    ++samples;
                }
            }

            if(samples == 0)
            {
                continue;
            }

            std::uint8_t * targetPixel = result.pixels.data() + (y * result.width + x) * 4;
            targetPixel[0] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (redSum + samples / 2) / samples));
            targetPixel[1] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (greenSum + samples / 2) / samples));
            targetPixel[2] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (blueSum + samples / 2) / samples));
            targetPixel[3] = static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (alphaSum + samples / 2) / samples));
        }
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::premultiplyPixels(std::vector<std::uint8_t> * const _pixels)
{
    if(_pixels == nullptr)
    {
        return;
    }

    const NSUInteger count = _pixels->size() / 4;
    for(NSUInteger index = 0; index != count; ++index)
    {
        std::uint8_t * pixel = _pixels->data() + index * 4;
        const std::uint32_t alpha = pixel[3];
        if(alpha == 255)
        {
            continue;
        }

        if(alpha == 0)
        {
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = 0;
            continue;
        }

        pixel[0] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel[0]) * alpha + 127) / 255);
        pixel[1] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel[1]) * alpha + 127) / 255);
        pixel[2] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel[2]) * alpha + 127) / 255);
    }
}

//////////////////////////////////////////////////////////////////////////
std::vector<MetalRenderBackend::MipPixelsDesc> MetalRenderBackend::buildPremultipliedMipChain(std::vector<std::uint8_t> _pixels, NSUInteger _width, NSUInteger _height)
{
    std::vector<MipPixelsDesc> levels;
    if(_pixels.empty() == true || _width == 0 || _height == 0)
    {
        return levels;
    }

    MipPixelsDesc base;
    base.width = _width;
    base.height = _height;
    base.pixels = std::move(_pixels);
    levels.emplace_back(std::move(base));

    while(levels.back().width > 1 || levels.back().height > 1)
    {
        levels.emplace_back(makeNextMipLevel(levels.back()));
    }

    return levels;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureFromMipChain(std::vector<MetalRenderBackend::MipPixelsDesc> _levels)
{
    if(_levels.empty() == true || _levels.front().pixels.empty() == true || _levels.front().width == 0 || _levels.front().height == 0)
    {
        return nil;
    }

    MTLTextureDescriptor * desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:_levels.front().width
                                                                                    height:_levels.front().height
                                                                                 mipmapped:_levels.size() > 1];
    desc.mipmapLevelCount = _levels.size();
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;

    id<MTLTexture> texture = [m_device newTextureWithDescriptor:desc];
    if(texture == nil)
    {
        return nil;
    }

    for(NSUInteger level = 0; level != _levels.size(); ++level)
    {
        const MipPixelsDesc & mip = _levels[level];
        [texture replaceRegion:MTLRegionMake2D(0, 0, mip.width, mip.height)
                    mipmapLevel:level
                      withBytes:mip.pixels.data()
                    bytesPerRow:mip.width * 4];
    }

    return texture;
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::bleedTransparentPixels(std::vector<std::uint8_t> * const _pixels, NSUInteger _width, NSUInteger _height)
{
    if(_pixels == nullptr || _pixels->empty() == true || _width == 0 || _height == 0)
    {
        return;
    }

    const NSUInteger count = _width * _height;
    std::vector<std::uint8_t> filled(count, 0);
    bool hasTransparent = false;
    bool hasOpaqueSource = false;
    for(NSUInteger index = 0; index != count; ++index)
    {
        const std::uint8_t alpha = (*_pixels)[index * 4 + 3];
        if(alpha == 0)
        {
            hasTransparent = true;
            continue;
        }

        filled[index] = 1;
        hasOpaqueSource = true;
    }

    if(hasTransparent == false || hasOpaqueSource == false)
    {
        return;
    }

    constexpr NSUInteger maxPasses = 64;
    for(NSUInteger pass = 0; pass != maxPasses; ++pass)
    {
        bool changed = false;
        bool hasUnfilled = false;
        std::vector<std::uint8_t> nextPixels = *_pixels;
        std::vector<std::uint8_t> nextFilled = filled;

        for(NSUInteger y = 0; y != _height; ++y)
        {
            for(NSUInteger x = 0; x != _width; ++x)
            {
                const NSUInteger index = y * _width + x;
                if(filled[index] != 0)
                {
                    continue;
                }

                std::uint32_t red = 0;
                std::uint32_t green = 0;
                std::uint32_t blue = 0;
                std::uint32_t samples = 0;
                const NSInteger xi = static_cast<NSInteger>(x);
                const NSInteger yi = static_cast<NSInteger>(y);
                for(NSInteger oy = -1; oy <= 1; ++oy)
                {
                    const NSInteger ny = yi + oy;
                    if(ny < 0 || ny >= static_cast<NSInteger>(_height))
                    {
                        continue;
                    }

                    for(NSInteger ox = -1; ox <= 1; ++ox)
                    {
                        const NSInteger nx = xi + ox;
                        if((ox == 0 && oy == 0) || nx < 0 || nx >= static_cast<NSInteger>(_width))
                        {
                            continue;
                        }

                        const NSUInteger neighbor = static_cast<NSUInteger>(ny) * _width + static_cast<NSUInteger>(nx);
                        if(filled[neighbor] == 0)
                        {
                            continue;
                        }

                        const std::uint8_t * pixel = _pixels->data() + neighbor * 4;
                        red += pixel[0];
                        green += pixel[1];
                        blue += pixel[2];
                        ++samples;
                    }
                }

                if(samples == 0)
                {
                    hasUnfilled = true;
                    continue;
                }

                std::uint8_t * pixel = nextPixels.data() + index * 4;
                pixel[0] = static_cast<std::uint8_t>(red / samples);
                pixel[1] = static_cast<std::uint8_t>(green / samples);
                pixel[2] = static_cast<std::uint8_t>(blue / samples);
                nextFilled[index] = 1;
                changed = true;
            }
        }

        *_pixels = std::move(nextPixels);
        filled = std::move(nextFilled);
        if(changed == false || hasUnfilled == false)
        {
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::fillTransparentPixelsWithAverageColor(std::vector<std::uint8_t> * const _pixels)
{
    if(_pixels == nullptr || _pixels->empty() == true)
    {
        return;
    }

    std::uint64_t red = 0;
    std::uint64_t green = 0;
    std::uint64_t blue = 0;
    std::uint64_t weight = 0;
    const NSUInteger count = _pixels->size() / 4;
    for(NSUInteger index = 0; index != count; ++index)
    {
        const std::uint8_t * pixel = _pixels->data() + index * 4;
        const std::uint8_t alpha = pixel[3];
        if(alpha == 0)
        {
            continue;
        }

        red += static_cast<std::uint64_t>(pixel[0]) * alpha;
        green += static_cast<std::uint64_t>(pixel[1]) * alpha;
        blue += static_cast<std::uint64_t>(pixel[2]) * alpha;
        weight += alpha;
    }

    if(weight == 0)
    {
        return;
    }

    const std::uint8_t averageRed = static_cast<std::uint8_t>(red / weight);
    const std::uint8_t averageGreen = static_cast<std::uint8_t>(green / weight);
    const std::uint8_t averageBlue = static_cast<std::uint8_t>(blue / weight);
    for(NSUInteger index = 0; index != count; ++index)
    {
        std::uint8_t * pixel = _pixels->data() + index * 4;
        if(pixel[3] != 0)
        {
            continue;
        }

        pixel[0] = averageRed;
        pixel[1] = averageGreen;
        pixel[2] = averageBlue;
    }
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureFromImage(NSImage * _image, NSString * _cacheKey)
{
    if(_image == nil)
    {
        return nil;
    }

    if(_cacheKey != nil)
    {
        id<MTLTexture> cached = [m_textureCache objectForKey:_cacheKey];
        if(cached != nil)
        {
            return cached;
        }
    }

    NSRect proposed = NSMakeRect(0.0, 0.0, _image.size.width, _image.size.height);
    CGImageRef cgImage = [_image CGImageForProposedRect:&proposed context:nil hints:nil];
    if(cgImage == nullptr)
    {
        return nil;
    }

    const NSUInteger width = CGImageGetWidth(cgImage);
    const NSUInteger height = CGImageGetHeight(cgImage);
    if(width == 0 || height == 0)
    {
        return nil;
    }

    std::vector<std::uint8_t> pixels(width * height * 4);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels.data(),
                                                 width,
                                                 height,
                                                 8,
                                                 width * 4,
                                                 colorSpace,
                                                 kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colorSpace);
    if(context == nullptr)
    {
        return nil;
    }

    CGContextClearRect(context, CGRectMake(0.0, 0.0, width, height));
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), cgImage);
    CGContextRelease(context);

    id<MTLTexture> texture = this->textureFromMipChain(buildPremultipliedMipChain(std::move(pixels), width, height));

    if(_cacheKey != nil && texture != nil)
    {
        [m_textureCache setObject:texture forKey:_cacheKey];
    }

    return texture;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureFromImagePixels(std::vector<std::uint8_t> _pixels, NSUInteger _width, NSUInteger _height, NSString * _cacheKey)
{
    if(_pixels.empty() == true || _width == 0 || _height == 0)
    {
        return nil;
    }

    if(_cacheKey != nil)
    {
        id<MTLTexture> cached = [m_textureCache objectForKey:_cacheKey];
        if(cached != nil)
        {
            return cached;
        }
    }

    premultiplyPixels(&_pixels);

    id<MTLTexture> texture = this->textureFromMipChain(buildPremultipliedMipChain(std::move(_pixels), _width, _height));
    if(texture == nil)
    {
        return nil;
    }

    if(_cacheKey != nil)
    {
        [m_textureCache setObject:texture forKey:_cacheKey];
    }

    return texture;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureFromRgbaPixels(const std::vector<std::uint8_t> & _pixels, NSUInteger _width, NSUInteger _height)
{
    if(_pixels.empty() == true || _width == 0 || _height == 0)
    {
        return nil;
    }

    std::vector<std::uint8_t> pixels = _pixels;
    premultiplyPixels(&pixels);

    id<MTLTexture> texture = this->textureFromMipChain(buildPremultipliedMipChain(std::move(pixels), _width, _height));
    if(texture == nil)
    {
        return nil;
    }

    return texture;
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureForCommand(Figma::DocumentInterface * _document, const Figma::RenderCommand & _command)
{
    if(_document == nullptr || _command.assetId.empty() == true)
    {
        return nil;
    }

    const Figma::AssetDesc * asset = _document->findAsset(std::string_view(_command.assetId.data(), _command.assetId.size()));
    if(asset == nullptr || asset->bytes.empty() == true)
    {
        return nil;
    }

    NSString * cacheKey = metalTextureCacheKey(_command);
    DecodedImagePixelsDesc image;
    if(decodeAssetPixels(*asset, &image) == false)
    {
        return nil;
    }

    applyImageFilterToPixels(&image, _command);
    return this->textureFromImagePixels(std::move(image.pixels), image.width, image.height, cacheKey);
}

//////////////////////////////////////////////////////////////////////////
id<MTLTexture> MetalRenderBackend::textureForText(FreeTypeTextRenderer * _textRenderer, const Figma::RenderCommand & _command, CGFloat _pixelScale)
{
    if(_textRenderer == nullptr)
    {
        return nil;
    }

    NSSize pointSize = NSMakeSize(std::max<CGFloat>(1.0, _command.rect.w), std::max<CGFloat>(1.0, _command.rect.h));
    std::vector<std::uint8_t> pixels;
    NSUInteger width = 0;
    NSUInteger height = 0;
    if(_textRenderer->makeTextPixels(_command, pointSize, _pixelScale, &pixels, &width, &height) == false)
    {
        return nil;
    }

    return this->textureFromRgbaPixels(pixels, width, height);
}

//////////////////////////////////////////////////////////////////////////
void MetalRenderBackend::appendQuadVertices(std::vector<MetalVertexDesc> * const _vertices, const Figma::RenderCommand & _command)
{
    const float x0 = _command.rect.x;
    const float y0 = _command.rect.y;
    const float x1 = _command.rect.x + _command.rect.w;
    const float y1 = _command.rect.y + _command.rect.h;
    const bool textured = _command.type == Figma::ERenderCommandType::Image || _command.type == Figma::ERenderCommandType::Text;
    const float red = textured == true ? 1.0f : _command.color.r;
    const float green = textured == true ? 1.0f : _command.color.g;
    const float blue = textured == true ? 1.0f : _command.color.b;
    const float alpha = textured == true ? 1.0f : _command.color.a;
    const std::array<MetalVertexDesc, 4> quad = {{
        {{x0, y0}, {0.0f, 0.0f}, {red, green, blue, alpha}},
        {{x1, y0}, {1.0f, 0.0f}, {red, green, blue, alpha}},
        {{x0, y1}, {0.0f, 1.0f}, {red, green, blue, alpha}},
        {{x1, y1}, {1.0f, 1.0f}, {red, green, blue, alpha}},
    }};
    _vertices->insert(_vertices->end(), quad.begin(), quad.end());
}

//////////////////////////////////////////////////////////////////////////
bool MetalRenderBackend::buildCommandGeometry(const Figma::RenderCommand & _command, std::vector<MetalVertexDesc> * const _vertices, std::vector<std::uint16_t> * const _indices)
{
    _vertices->clear();
    _indices->clear();

    if(_command.vertices.empty() == false && _command.indices.size() >= 3)
    {
        _vertices->reserve(_command.vertices.size());
        for(const Figma::RenderVertex & vertex : _command.vertices)
        {
            MetalVertexDesc metalVertex = {};
            metalVertex.position[0] = vertex.x;
            metalVertex.position[1] = vertex.y;
            metalVertex.uv[0] = vertex.u;
            metalVertex.uv[1] = vertex.v;
            if(_command.type == Figma::ERenderCommandType::Image || _command.type == Figma::ERenderCommandType::Text)
            {
                metalVertex.color[0] = 1.0f;
                metalVertex.color[1] = 1.0f;
                metalVertex.color[2] = 1.0f;
                metalVertex.color[3] = 1.0f;
            }
            else
            {
                metalVertex.color[0] = vertex.color.r;
                metalVertex.color[1] = vertex.color.g;
                metalVertex.color[2] = vertex.color.b;
                metalVertex.color[3] = vertex.color.a;
            }
            _vertices->emplace_back(metalVertex);
        }

        _indices->assign(_command.indices.begin(), _command.indices.end());
        return true;
    }

    if(_command.type == Figma::ERenderCommandType::Fill ||
       _command.type == Figma::ERenderCommandType::Stroke ||
       _command.type == Figma::ERenderCommandType::Text ||
       _command.type == Figma::ERenderCommandType::Image ||
       _command.type == Figma::ERenderCommandType::DebugHotspot)
    {
        appendQuadVertices(_vertices, _command);
        *_indices = {0, 1, 2, 2, 1, 3};
        return true;
    }

    return false;
}

void MetalRenderBackend::drawVertices(id<MTLCommandBuffer> _commandBuffer,
                  id<MTLTexture> _target,
                  id<MTLTexture> _backdrop,
                  id<MTLTexture> _sourceTexture,
                  const std::vector<MetalVertexDesc> & _vertices,
                  const std::vector<std::uint16_t> & _indices,
                  const MetalUniformDesc & _uniforms)
{
    if(_vertices.empty() == true || _indices.empty() == true)
    {
        return;
    }

    this->copyTextureToTexture(_commandBuffer, _target, _backdrop);

    MTLRenderPassDescriptor * pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = _target;
    pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [_commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:m_commandPipeline];
    id<MTLBuffer> vertexBuffer = [m_device newBufferWithBytes:_vertices.data() length:_vertices.size() * sizeof(MetalVertexDesc) options:MTLResourceStorageModeShared];
    id<MTLBuffer> indexBuffer = [m_device newBufferWithBytes:_indices.data() length:_indices.size() * sizeof(std::uint16_t) options:MTLResourceStorageModeShared];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:1];
    [encoder setFragmentBytes:&_uniforms length:sizeof(_uniforms) atIndex:0];
    [encoder setFragmentTexture:_sourceTexture != nil ? _sourceTexture : m_whiteTexture atIndex:0];
    [encoder setFragmentTexture:_backdrop atIndex:1];
    [encoder setFragmentSamplerState:m_sampler atIndex:0];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_indices.size()
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:indexBuffer
                 indexBufferOffset:0];
    [encoder endEncoding];
}

MetalUniformDesc MetalRenderBackend::makeUniforms(const Figma::RenderCommand & _command,
                              bool _hasTexture,
                              CGFloat _viewportWidth,
                              CGFloat _viewportHeight,
                              NSUInteger _targetWidth,
                              NSUInteger _targetHeight) const
{
    MetalUniformDesc uniforms = {};
    uniforms.viewportSize[0] = static_cast<float>(_viewportWidth);
    uniforms.viewportSize[1] = static_cast<float>(_viewportHeight);
    uniforms.targetSize[0] = static_cast<float>(_targetWidth);
    uniforms.targetSize[1] = static_cast<float>(_targetHeight);
    uniforms.commandRect[0] = _command.rect.x;
    uniforms.commandRect[1] = _command.rect.y;
    uniforms.commandRect[2] = _command.rect.w;
    uniforms.commandRect[3] = _command.rect.h;
    uniforms.opacity = _command.opacity;
    uniforms.hasTexture = _hasTexture == true ? 1U : 0U;
    uniforms.shape = static_cast<std::uint32_t>(_command.type == Figma::ERenderCommandType::Mesh ? EMetalShapeType::Rectangle : metalShapeForCommand(_command));
    uniforms.blendMode = static_cast<std::uint32_t>(metalBlendModeForCommand(_command));
    uniforms.pad1[0] = _command.cornerRadius;
    return uniforms;
}

//////////////////////////////////////////////////////////////////////////
bool MetalRenderBackend::shouldSkipCommand(const Figma::RenderCommand & _command) const
{
    if(_command.opacity <= 0.0f)
    {
        return true;
    }

    if(_command.type != Figma::ERenderCommandType::Image && _command.color.a <= 0.0f)
    {
        return true;
    }

    if(metalBlendModeIsSupported(_command) == false)
    {
        return true;
    }

    if(_command.type == Figma::ERenderCommandType::ClipBegin || _command.type == Figma::ERenderCommandType::ClipEnd)
    {
        return true;
    }

    return false;
}

void MetalRenderBackend::drawCommand(id<MTLCommandBuffer> _commandBuffer,
                 Figma::DocumentInterface * _document,
                 FreeTypeTextRenderer * _textRenderer,
                 const Figma::RenderCommand & _command,
                 id<MTLTexture> _target,
                 id<MTLTexture> _backdrop,
                 CGFloat _viewportWidth,
                 CGFloat _viewportHeight,
                 NSUInteger _targetWidth,
                 NSUInteger _targetHeight)
{
    if(this->shouldSkipCommand(_command) == true)
    {
        return;
    }

    id<MTLTexture> sourceTexture = nil;
    bool hasTexture = false;
    if(_command.type == Figma::ERenderCommandType::Image)
    {
        sourceTexture = this->textureForCommand(_document, _command);
        if(sourceTexture == nil)
        {
            return;
        }
        hasTexture = true;
    }
    else if(_command.type == Figma::ERenderCommandType::Text)
    {
        const CGFloat rasterScale = static_cast<CGFloat>(_targetWidth) / std::max<CGFloat>(1.0, _viewportWidth);
        sourceTexture = this->textureForText(_textRenderer, _command, rasterScale);
        if(sourceTexture == nil)
        {
            return;
        }
        hasTexture = true;
    }
    else if(_command.type == Figma::ERenderCommandType::DebugHotspot)
    {
        return;
    }

    std::vector<MetalVertexDesc> vertices;
    std::vector<std::uint16_t> indices;
    if(this->buildCommandGeometry(_command, &vertices, &indices) == false)
    {
        return;
    }

    MetalUniformDesc uniforms = this->makeUniforms(_command, hasTexture, _viewportWidth, _viewportHeight, _targetWidth, _targetHeight);
    this->drawVertices(_commandBuffer, _target, _backdrop, sourceTexture, vertices, indices, uniforms);
}

void MetalRenderBackend::drawLayerTexture(id<MTLCommandBuffer> _commandBuffer,
                      id<MTLTexture> _layerTexture,
                      id<MTLTexture> _target,
                      id<MTLTexture> _backdrop,
                      CGFloat _opacity,
                      CGFloat _viewportWidth,
                      CGFloat _viewportHeight,
                      NSUInteger _targetWidth,
                      NSUInteger _targetHeight)
{
    Figma::RenderCommand command;
    command.type = Figma::ERenderCommandType::Image;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.w = static_cast<float>(_viewportWidth);
    command.rect.h = static_cast<float>(_viewportHeight);
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = 1.0f;
    command.opacity = static_cast<float>(std::clamp<CGFloat>(_opacity, 0.0, 1.0));

    std::vector<MetalVertexDesc> vertices;
    std::vector<std::uint16_t> indices;
    appendQuadVertices(&vertices, command);
    indices = {0, 1, 2, 2, 1, 3};
    MetalUniformDesc uniforms = this->makeUniforms(command, true, _viewportWidth, _viewportHeight, _targetWidth, _targetHeight);
    this->drawVertices(_commandBuffer, _target, _backdrop, _layerTexture, vertices, indices, uniforms);
}

void MetalRenderBackend::renderCommandRange(id<MTLCommandBuffer> _commandBuffer,
                        Figma::DocumentInterface * _document,
                        FreeTypeTextRenderer * _textRenderer,
                        const Figma::RenderCommandVector & _commands,
                        std::size_t _begin,
                        std::size_t _end,
                        const std::vector<std::uint8_t> & _visibility,
                        id<MTLTexture> _target,
                        id<MTLTexture> _backdrop,
                        CGFloat _viewportWidth,
                        CGFloat _viewportHeight,
                        NSUInteger _targetWidth,
                        NSUInteger _targetHeight,
                        bool _allowLayers)
{
    for(std::size_t index = _begin; index != _end;)
    {
        const Figma::RenderCommand & command = _commands[index];
        if(index < _visibility.size() && _visibility[index] == 0)
        {
            ++index;
            continue;
        }

        if(_allowLayers == true && command.renderLayerId != 0)
        {
            const std::uint32_t layerId = command.renderLayerId;
            const CGFloat layerOpacity = static_cast<CGFloat>(command.renderLayerOpacity);
            const std::size_t layerBegin = index;
            while(index != _end && _commands[index].renderLayerId == layerId)
            {
                ++index;
            }

            if(layerOpacity <= 0.0)
            {
                continue;
            }

            id<MTLTexture> layerTexture = this->makeRenderTexture(_targetWidth, _targetHeight);
            id<MTLTexture> layerBackdrop = this->makeRenderTexture(_targetWidth, _targetHeight);
            this->clearTexture(_commandBuffer, layerTexture, MTLClearColorMake(0.0, 0.0, 0.0, 0.0));
            this->renderCommandRange(_commandBuffer,
                                     _document,
                                     _textRenderer,
                                     _commands,
                                     layerBegin,
                                     index,
                                     _visibility,
                                     layerTexture,
                                     layerBackdrop,
                                     _viewportWidth,
                                     _viewportHeight,
                                     _targetWidth,
                                     _targetHeight,
                                     false);
            this->drawLayerTexture(_commandBuffer, layerTexture, _target, _backdrop, layerOpacity, _viewportWidth, _viewportHeight, _targetWidth, _targetHeight);
            continue;
        }

        this->drawCommand(_commandBuffer, _document, _textRenderer, command, _target, _backdrop, _viewportWidth, _viewportHeight, _targetWidth, _targetHeight);
        ++index;
    }
}

void MetalRenderBackend::copyTexture(id<MTLCommandBuffer> _commandBuffer,
                 id<MTLTexture> _source,
                 id<MTLTexture> _destination,
                 CGFloat _viewportWidth,
                 CGFloat _viewportHeight,
                 NSUInteger _targetWidth,
                 NSUInteger _targetHeight)
{
    MTLRenderPassDescriptor * pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = _destination;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    Figma::RenderCommand command;
    command.rect.x = 0.0f;
    command.rect.y = 0.0f;
    command.rect.w = static_cast<float>(_viewportWidth);
    command.rect.h = static_cast<float>(_viewportHeight);
    command.color.r = 1.0f;
    command.color.g = 1.0f;
    command.color.b = 1.0f;
    command.color.a = 1.0f;

    std::vector<MetalVertexDesc> vertices;
    appendQuadVertices(&vertices, command);
    const std::uint16_t indices[] = {0, 1, 2, 2, 1, 3};
    MetalUniformDesc uniforms = this->makeUniforms(command, true, _viewportWidth, _viewportHeight, _targetWidth, _targetHeight);

    id<MTLRenderCommandEncoder> encoder = [_commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:m_copyPipeline];
    id<MTLBuffer> vertexBuffer = [m_device newBufferWithBytes:vertices.data() length:vertices.size() * sizeof(MetalVertexDesc) options:MTLResourceStorageModeShared];
    id<MTLBuffer> indexBuffer = [m_device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
    [encoder setFragmentTexture:_source atIndex:0];
    [encoder setFragmentSamplerState:m_sampler atIndex:0];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:6
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:indexBuffer
                 indexBufferOffset:0];
    [encoder endEncoding];
}
