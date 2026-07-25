#include "FreeTypeTextRenderer.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace
{
    //////////////////////////////////////////////////////////////////////////
    static std::string stdString(const std::string & _value)
    {
        return std::string(_value.data(), _value.size());
    }

    //////////////////////////////////////////////////////////////////////////
    static NSString * nsString(const std::string & _value)
    {
        return [[NSString alloc] initWithBytes:_value.data() length:_value.size() encoding:NSUTF8StringEncoding] ?: @"";
    }

    //////////////////////////////////////////////////////////////////////////
    static std::string stdString(NSString * _value)
    {
        const char * const value = _value.UTF8String;
        return value != nullptr ? std::string(value) : std::string();
    }

    //////////////////////////////////////////////////////////////////////////
    static NSCompositingOperation compositingOperationForCommand(const ViewerRenderCommand & _command)
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
}

//////////////////////////////////////////////////////////////////////////
FreeTypeTextRenderer::FreeTypeTextRenderer()
{
    if(FT_Init_FreeType(&m_library) != FT_Err_Ok)
    {
        m_library = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
FreeTypeTextRenderer::~FreeTypeTextRenderer()
{
    this->clearFontCache();

    if(m_library != nullptr)
    {
        FT_Done_FreeType(m_library);
    }
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::drawText(const ViewerRenderCommand & _command, NSRect _rect)
{
    this->drawTextAtRasterScale(_command, _rect, pixelScaleForCurrentContext());
}

//////////////////////////////////////////////////////////////////////////
bool FreeTypeTextRenderer::makeTextPixels(const ViewerRenderCommand & _command, NSSize _pointSize, CGFloat _rasterScale, std::vector<std::uint8_t> * const _pixels, NSUInteger * const _width, NSUInteger * const _height)
{
    if(m_library == nullptr || _command.text.empty() == true || _command.color.a <= 0.0f || _pixels == nullptr || _width == nullptr || _height == nullptr)
    {
        return false;
    }

    FT_Face face = this->faceForCommand(_command);
    if(face == nullptr)
    {
        return false;
    }

    const CGFloat rasterScale = std::max<CGFloat>(1.0, _rasterScale);
    const NSUInteger pixelWidth = static_cast<NSUInteger>(std::ceil(std::max<CGFloat>(1.0, _pointSize.width) * rasterScale));
    const NSUInteger pixelHeight = static_cast<NSUInteger>(std::ceil(std::max<CGFloat>(1.0, _pointSize.height) * rasterScale));
    const FT_UInt pixelSize = static_cast<FT_UInt>(std::max<long>(1, std::lround(static_cast<CGFloat>(_command.fontSize) * rasterScale)));
    if(FT_Set_Pixel_Sizes(face, 0, pixelSize) != FT_Err_Ok)
    {
        return false;
    }

    if(_command.textLines.empty() == true)
    {
        return false;
    }

    ViewerRenderCommand textCommand = _command;
    textCommand.opacity = 1.0f;

    _pixels->assign(pixelWidth * pixelHeight * 4, 0);
    this->rasterizeDecodedLines(face, textCommand, rasterScale, _pixels->data(), pixelWidth, pixelHeight, pixelWidth * 4);

    *_width = pixelWidth;
    *_height = pixelHeight;
    return true;
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::addFontSearchDirectory(NSString * _directory)
{
    if(_directory.length == 0)
    {
        return;
    }

    NSString * path = [[_directory stringByExpandingTildeInPath] stringByStandardizingPath];
    const std::string directory = stdString(path);
    if(directory.empty() == true)
    {
        return;
    }

    if(std::find(m_fontDirectories.begin(), m_fontDirectories.end(), directory) != m_fontDirectories.end())
    {
        return;
    }

    m_fontDirectories.emplace_back(directory);
    this->clearFontCache();
    this->clearMissingFonts();
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::clearFontCache()
{
    for(auto & entry : m_faces)
    {
        if(entry.second != nullptr)
        {
            FT_Done_Face(entry.second);
        }
    }

    m_faces.clear();
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::clearMissingFonts()
{
    m_missingFonts.clear();
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::collectMissingFonts(const ViewerRenderCommandVector & _commands)
{
    this->clearMissingFonts();

    for(const ViewerRenderCommand & command : _commands)
    {
        if(command.type != FIGMA_RENDER_COMMAND_TEXT)
        {
            continue;
        }

        (void)this->faceForCommand(command);
    }
}

//////////////////////////////////////////////////////////////////////////
NSArray<NSString *> * FreeTypeTextRenderer::missingFontDescriptions() const
{
    std::vector<std::string> descriptions;
    descriptions.reserve(m_missingFonts.size());

    for(const auto & entry : m_missingFonts)
    {
        descriptions.emplace_back(entry.second);
    }

    std::sort(descriptions.begin(), descriptions.end());

    NSMutableArray<NSString *> * result = [NSMutableArray arrayWithCapacity:descriptions.size()];
    for(const std::string & description : descriptions)
    {
        [result addObject:[NSString stringWithUTF8String:description.c_str()] ?: @""];
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
NSArray<NSString *> * FreeTypeTextRenderer::fontSearchDirectories() const
{
    NSMutableArray<NSString *> * directories = [NSMutableArray array];

    for(const std::string & directory : m_fontDirectories)
    {
        [directories addObject:[NSString stringWithUTF8String:directory.c_str()] ?: @""];
    }

    [directories addObjectsFromArray:environmentFontSearchDirectories()];
    [directories addObjectsFromArray:defaultFontSearchDirectories()];
    return directories;
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::drawTextAtRasterScale(const ViewerRenderCommand & _command, NSRect _rect, CGFloat _rasterScale)
{
    if(m_library == nullptr || _command.text.empty() == true || _command.opacity <= 0.0f || _command.color.a <= 0.0f)
    {
        return;
    }

    FT_Face face = this->faceForCommand(_command);
    if(face == nullptr)
    {
        return;
    }

    const CGFloat rasterScale = std::max<CGFloat>(1.0, _rasterScale);
    const FT_UInt pixelSize = static_cast<FT_UInt>(std::max<long>(1, std::lround(static_cast<CGFloat>(_command.fontSize) * rasterScale)));
    if(FT_Set_Pixel_Sizes(face, 0, pixelSize) != FT_Err_Ok)
    {
        return;
    }

    if(_command.textLines.empty() == true)
    {
        return;
    }

    this->drawDecodedLines(face, _command, _rect, rasterScale);
}

//////////////////////////////////////////////////////////////////////////
std::vector<char32_t> FreeTypeTextRenderer::decodeUtf8(std::string_view _text)
{
    std::vector<char32_t> result;
    result.reserve(_text.size());

    const unsigned char * cursor = reinterpret_cast<const unsigned char *>(_text.data());
    const unsigned char * const end = cursor + _text.size();
    while(cursor < end)
    {
        const unsigned char c = *cursor++;
        if(c < 0x80)
        {
            result.emplace_back(static_cast<char32_t>(c));
            continue;
        }

        char32_t codepoint = U'?';
        int continuation = 0;
        if((c & 0xE0) == 0xC0)
        {
            codepoint = c & 0x1F;
            continuation = 1;
        }
        else if((c & 0xF0) == 0xE0)
        {
            codepoint = c & 0x0F;
            continuation = 2;
        }
        else if((c & 0xF8) == 0xF0)
        {
            codepoint = c & 0x07;
            continuation = 3;
        }

        bool valid = continuation != 0 && cursor + continuation <= end;
        for(int index = 0; valid == true && index != continuation; ++index)
        {
            const unsigned char part = *cursor++;
            if((part & 0xC0) != 0x80)
            {
                valid = false;
                break;
            }

            codepoint = (codepoint << 6) | (part & 0x3F);
        }

        result.emplace_back(valid == true ? codepoint : U'?');
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
CGFloat FreeTypeTextRenderer::to26Dot6(FT_Pos _value)
{
    return static_cast<CGFloat>(_value) / 64.0;
}

//////////////////////////////////////////////////////////////////////////
FT_UInt FreeTypeTextRenderer::glyphIndexForCodepoint(FT_Face _face, char32_t _codepoint)
{
    FT_UInt glyphIndex = FT_Get_Char_Index(_face, static_cast<FT_ULong>(_codepoint));
    if(glyphIndex == 0)
    {
        glyphIndex = FT_Get_Char_Index(_face, static_cast<FT_ULong>(U'?'));
    }

    return glyphIndex;
}

//////////////////////////////////////////////////////////////////////////
CGFloat FreeTypeTextRenderer::measureLineAdvance(FT_Face _face, const std::vector<char32_t> & _codepoints)
{
    CGFloat advance = 0.0;
    FT_UInt previousGlyph = 0;

    for(char32_t codepoint : _codepoints)
    {
        if(codepoint == U'\n' || codepoint == U'\r')
        {
            continue;
        }

        const FT_UInt glyphIndex = glyphIndexForCodepoint(_face, codepoint);
        if(previousGlyph != 0 && glyphIndex != 0 && FT_HAS_KERNING(_face) != 0)
        {
            FT_Vector kerning;
            if(FT_Get_Kerning(_face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &kerning) == FT_Err_Ok)
            {
                advance += to26Dot6(kerning.x);
            }
        }

        if(FT_Load_Glyph(_face, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_NO_AUTOHINT | FT_LOAD_COLOR) == FT_Err_Ok)
        {
            advance += to26Dot6(_face->glyph->metrics.horiAdvance);
        }

        previousGlyph = glyphIndex;
    }

    return advance;
}

//////////////////////////////////////////////////////////////////////////
CGFloat FreeTypeTextRenderer::horizontalScaleForLine(FT_Face _face, const std::vector<char32_t> & _codepoints, CGFloat _targetWidth)
{
    const CGFloat measuredWidth = measureLineAdvance(_face, _codepoints);
    if(measuredWidth <= 0.001 || _targetWidth <= 0.001)
    {
        return 1.0;
    }

    const CGFloat scale = _targetWidth / measuredWidth;
    if(scale < 1.0 && scale > 0.94)
    {
        return 1.0;
    }

    return std::clamp<CGFloat>(scale, 0.25, 4.0);
}

//////////////////////////////////////////////////////////////////////////
CGFloat FreeTypeTextRenderer::verticalScaleForLine(FT_Face _face, const ViewerRenderCommand & _command, const ViewerRenderTextLineDesc & _line, CGFloat _rasterScale)
{
    if(_face == nullptr || _line.lineAscent <= 0.0f || _rasterScale <= 0.0)
    {
        return 1.0;
    }

    if(normalizedFontName(stdString(_command.fontFamily)) != "papyrus")
    {
        return 1.0;
    }

    const CGFloat faceAscent = to26Dot6(_face->size->metrics.ascender);
    const CGFloat targetAscent = static_cast<CGFloat>(_line.lineAscent) * _rasterScale;
    if(faceAscent <= 0.001 || targetAscent <= 0.001)
    {
        return 1.0;
    }

    return std::clamp<CGFloat>(targetAscent / faceAscent, 0.85, 1.25);
}

//////////////////////////////////////////////////////////////////////////
std::string_view FreeTypeTextRenderer::trimTrailingWhitespace(std::string_view _view)
{
    while(_view.empty() == false && (_view.back() == ' ' || _view.back() == '\t' || _view.back() == '\r' || _view.back() == '\n'))
    {
        _view.remove_suffix(1);
    }

    return _view;
}

//////////////////////////////////////////////////////////////////////////
std::string_view FreeTypeTextRenderer::explicitLineSegment(std::string_view _view, std::size_t _lineIndex)
{
    std::size_t begin = 0;
    std::size_t index = 0;
    while(begin <= _view.size())
    {
        const std::size_t end = _view.find_first_of("\r\n", begin);
        if(index == _lineIndex)
        {
            return end == std::string_view::npos ? _view.substr(begin) : _view.substr(begin, end - begin);
        }

        if(end == std::string_view::npos)
        {
            break;
        }

        begin = end + 1;
        if(_view[end] == '\r' && begin < _view.size() && _view[begin] == '\n')
        {
            ++begin;
        }

        ++index;
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
std::string_view FreeTypeTextRenderer::sourceTextView(const ViewerRenderCommand & _command, const ViewerRenderTextLineDesc & _line)
{
    const std::string & source = _line.text.empty() == false ? _line.text : _command.text;
    return std::string_view(source.data(), source.size());
}

//////////////////////////////////////////////////////////////////////////
std::string_view FreeTypeTextRenderer::textLineView(const ViewerRenderCommand & _command, std::size_t _lineIndex)
{
    const ViewerRenderTextLineDesc & line = _command.textLines[_lineIndex];
    std::string_view view = sourceTextView(_command, line);
    if(view.find_first_of("\r\n") == std::string_view::npos)
    {
        if(_lineIndex + 1 < _command.textLines.size())
        {
            std::string_view nextView = sourceTextView(_command, _command.textLines[_lineIndex + 1]);
            if(nextView.find_first_of("\r\n") != std::string_view::npos)
            {
                nextView = explicitLineSegment(nextView, _lineIndex + 1);
            }
            nextView = trimTrailingWhitespace(nextView);

            const std::size_t suffix = nextView.empty() == false ? view.rfind(nextView) : std::string_view::npos;
            if(suffix != std::string_view::npos && suffix > 0)
            {
                return trimTrailingWhitespace(view.substr(0, suffix));
            }
        }

        return trimTrailingWhitespace(view);
    }

    return trimTrailingWhitespace(explicitLineSegment(view, _lineIndex));
}

//////////////////////////////////////////////////////////////////////////
CGFloat FreeTypeTextRenderer::faceAscender(FT_Face _face, CGFloat _rasterScale)
{
    if(_face->size != nullptr)
    {
        return to26Dot6(_face->size->metrics.ascender) / std::max<CGFloat>(1.0, _rasterScale);
    }

    return 0.0;
}

//////////////////////////////////////////////////////////////////////////
std::uint8_t FreeTypeTextRenderer::coverageAt(const FT_Bitmap & _bitmap, NSInteger _x, NSInteger _y)
{
    if(_bitmap.buffer == nullptr || _x < 0 || _y < 0 || _x >= static_cast<NSInteger>(_bitmap.width) || _y >= static_cast<NSInteger>(_bitmap.rows))
    {
        return 0;
    }

    const NSInteger pitch = static_cast<NSInteger>(_bitmap.pitch);
    const unsigned char * sourceRow = pitch >= 0
        ? _bitmap.buffer + _y * pitch
        : _bitmap.buffer + (static_cast<NSInteger>(_bitmap.rows) - 1 - _y) * -pitch;

    switch(_bitmap.pixel_mode)
    {
    case FT_PIXEL_MODE_MONO:
        return (sourceRow[_x >> 3] & (0x80 >> (_x & 7))) != 0 ? 255 : 0;
    case FT_PIXEL_MODE_GRAY:
        return sourceRow[_x];
    case FT_PIXEL_MODE_GRAY2:
    {
        const unsigned char packed = sourceRow[_x >> 2];
        return static_cast<std::uint8_t>(((packed >> ((3 - (_x & 3)) * 2)) & 0x03) * 85);
    }
    case FT_PIXEL_MODE_GRAY4:
    {
        const unsigned char packed = sourceRow[_x >> 1];
        return static_cast<std::uint8_t>(((packed >> ((1 - (_x & 1)) * 4)) & 0x0F) * 17);
    }
    default:
        break;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::blendStraightPixel(unsigned char * const _target, CGFloat _red, CGFloat _green, CGFloat _blue, CGFloat _alpha)
{
    const CGFloat sourceAlpha = std::clamp<CGFloat>(_alpha, 0.0, 1.0);
    if(sourceAlpha <= 0.0)
    {
        return;
    }

    const CGFloat destinationAlpha = static_cast<CGFloat>(_target[3]) / 255.0;
    const CGFloat inverseAlpha = 1.0 - sourceAlpha;
    const CGFloat outputAlpha = sourceAlpha + destinationAlpha * inverseAlpha;

    CGFloat outputRed = 0.0;
    CGFloat outputGreen = 0.0;
    CGFloat outputBlue = 0.0;
    if(outputAlpha > 0.0)
    {
        const CGFloat destinationRed = static_cast<CGFloat>(_target[0]) / 255.0;
        const CGFloat destinationGreen = static_cast<CGFloat>(_target[1]) / 255.0;
        const CGFloat destinationBlue = static_cast<CGFloat>(_target[2]) / 255.0;

        outputRed = (_red * sourceAlpha + destinationRed * destinationAlpha * inverseAlpha) / outputAlpha;
        outputGreen = (_green * sourceAlpha + destinationGreen * destinationAlpha * inverseAlpha) / outputAlpha;
        outputBlue = (_blue * sourceAlpha + destinationBlue * destinationAlpha * inverseAlpha) / outputAlpha;
    }

    _target[0] = static_cast<std::uint8_t>(std::lround(std::clamp<CGFloat>(outputRed, 0.0, 1.0) * 255.0));
    _target[1] = static_cast<std::uint8_t>(std::lround(std::clamp<CGFloat>(outputGreen, 0.0, 1.0) * 255.0));
    _target[2] = static_cast<std::uint8_t>(std::lround(std::clamp<CGFloat>(outputBlue, 0.0, 1.0) * 255.0));
    _target[3] = static_cast<std::uint8_t>(std::lround(std::clamp<CGFloat>(outputAlpha, 0.0, 1.0) * 255.0));
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::blendGlyphBitmapPixel(const FT_Bitmap & _bitmap, NSInteger _sourceX, NSInteger _sourceY, unsigned char * const _target, const ViewerRenderCommand & _command)
{
    const CGFloat commandAlpha = std::clamp<CGFloat>(_command.color.a, 0.0, 1.0);
    if(commandAlpha <= 0.0)
    {
        return;
    }

    if(_bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
    {
        const NSInteger pitch = static_cast<NSInteger>(_bitmap.pitch);
        const unsigned char * sourceRow = pitch >= 0
            ? _bitmap.buffer + _sourceY * pitch
            : _bitmap.buffer + (static_cast<NSInteger>(_bitmap.rows) - 1 - _sourceY) * -pitch;
        const unsigned char * pixel = sourceRow + _sourceX * 4;
        const CGFloat sourceAlpha = static_cast<CGFloat>(pixel[3]) / 255.0 * commandAlpha;
        if(sourceAlpha <= 0.0)
        {
            return;
        }

        CGFloat red = static_cast<CGFloat>(pixel[2]) / 255.0;
        CGFloat green = static_cast<CGFloat>(pixel[1]) / 255.0;
        CGFloat blue = static_cast<CGFloat>(pixel[0]) / 255.0;
        if(pixel[3] != 0 && pixel[3] != 255)
        {
            const CGFloat invAlpha = 255.0 / static_cast<CGFloat>(pixel[3]);
            red = std::min<CGFloat>(1.0, red * invAlpha);
            green = std::min<CGFloat>(1.0, green * invAlpha);
            blue = std::min<CGFloat>(1.0, blue * invAlpha);
        }

        blendStraightPixel(_target, red, green, blue, sourceAlpha);
        return;
    }

    const std::uint8_t coverage = coverageAt(_bitmap, _sourceX, _sourceY);
    const CGFloat sourceAlpha = static_cast<CGFloat>(coverage) / 255.0 * commandAlpha;
    blendStraightPixel(_target,
                       std::clamp<CGFloat>(_command.color.r, 0.0, 1.0),
                       std::clamp<CGFloat>(_command.color.g, 0.0, 1.0),
                       std::clamp<CGFloat>(_command.color.b, 0.0, 1.0),
                       sourceAlpha);
}

//////////////////////////////////////////////////////////////////////////
NSImage * FreeTypeTextRenderer::makeGlyphImage(const FT_Bitmap & _bitmap, const ViewerRenderCommand & _command)
{
    if(_bitmap.width == 0 || _bitmap.rows == 0 || _bitmap.buffer == nullptr)
    {
        return nil;
    }

    const NSInteger width = static_cast<NSInteger>(_bitmap.width);
    const NSInteger height = static_cast<NSInteger>(_bitmap.rows);
    NSBitmapImageRep * imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                                           pixelsWide:width
                                                                           pixelsHigh:height
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSDeviceRGBColorSpace
                                                                          bitmapFormat:NSBitmapFormatThirtyTwoBitBigEndian
                                                                          bytesPerRow:0
                                                                         bitsPerPixel:0];
    if(imageRep == nil)
    {
        return nil;
    }

    unsigned char * target = [imageRep bitmapData];
    const NSInteger targetStride = [imageRep bytesPerRow];
    const CGFloat sourceAlpha = std::clamp<CGFloat>(_command.color.a * _command.opacity, 0.0, 1.0);
    const std::uint8_t red = static_cast<std::uint8_t>(std::lround(std::clamp<float>(_command.color.r, 0.0f, 1.0f) * 255.0f));
    const std::uint8_t green = static_cast<std::uint8_t>(std::lround(std::clamp<float>(_command.color.g, 0.0f, 1.0f) * 255.0f));
    const std::uint8_t blue = static_cast<std::uint8_t>(std::lround(std::clamp<float>(_command.color.b, 0.0f, 1.0f) * 255.0f));

    const int sourcePitch = _bitmap.pitch;
    for(NSInteger y = 0; y != height; ++y)
    {
        const unsigned char * sourceRow = sourcePitch >= 0
            ? _bitmap.buffer + y * sourcePitch
            : _bitmap.buffer + (height - 1 - y) * -sourcePitch;
        unsigned char * targetRow = target + y * targetStride;

        for(NSInteger x = 0; x != width; ++x)
        {
            std::uint8_t coverage = 0;
            std::uint8_t pixelRed = red;
            std::uint8_t pixelGreen = green;
            std::uint8_t pixelBlue = blue;
            switch(_bitmap.pixel_mode)
            {
            case FT_PIXEL_MODE_MONO:
            {
                coverage = (sourceRow[x >> 3] & (0x80 >> (x & 7))) != 0 ? 255 : 0;
                break;
            }
            case FT_PIXEL_MODE_GRAY:
            {
                coverage = sourceRow[x];
                break;
            }
            case FT_PIXEL_MODE_GRAY2:
            {
                const unsigned char packed = sourceRow[x >> 2];
                coverage = static_cast<std::uint8_t>(((packed >> ((3 - (x & 3)) * 2)) & 0x03) * 85);
                break;
            }
            case FT_PIXEL_MODE_GRAY4:
            {
                const unsigned char packed = sourceRow[x >> 1];
                coverage = static_cast<std::uint8_t>(((packed >> ((1 - (x & 1)) * 4)) & 0x0F) * 17);
                break;
            }
            case FT_PIXEL_MODE_BGRA:
            {
                const unsigned char * pixel = sourceRow + x * 4;
                pixelBlue = pixel[0];
                pixelGreen = pixel[1];
                pixelRed = pixel[2];
                coverage = pixel[3];
                break;
            }
            default:
                coverage = 0;
                break;
            }

            const std::uint8_t alpha = static_cast<std::uint8_t>(std::lround(static_cast<CGFloat>(coverage) * sourceAlpha));
            unsigned char * targetPixel = targetRow + x * 4;
            targetPixel[0] = static_cast<std::uint8_t>(std::lround(static_cast<CGFloat>(pixelRed) * static_cast<CGFloat>(alpha) / 255.0));
            targetPixel[1] = static_cast<std::uint8_t>(std::lround(static_cast<CGFloat>(pixelGreen) * static_cast<CGFloat>(alpha) / 255.0));
            targetPixel[2] = static_cast<std::uint8_t>(std::lround(static_cast<CGFloat>(pixelBlue) * static_cast<CGFloat>(alpha) / 255.0));
            targetPixel[3] = alpha;
        }
    }

    NSImage * image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [image addRepresentation:imageRep];
    return image;
}

//////////////////////////////////////////////////////////////////////////
FT_Face FreeTypeTextRenderer::faceForCommand(const ViewerRenderCommand & _command)
{
    const std::string key = this->fontKeyForCommand(_command);
    if(key.empty() == true)
    {
        return nullptr;
    }

    auto it = m_faces.find(key);
    if(it != m_faces.end())
    {
        if(it->second == nullptr)
        {
            this->recordMissingFont(_command, stdString(_command.fontPostscriptName), stdString(_command.fontFamily), stdString(_command.fontStyle));
        }

        return it->second;
    }

    FT_Face face = this->openFaceForCommand(_command);
    m_faces.emplace(key, face);
    return face;
}

//////////////////////////////////////////////////////////////////////////
std::string FreeTypeTextRenderer::fontKeyForCommand(const ViewerRenderCommand & _command) const
{
    if(_command.fontPostscriptName.empty() == false)
    {
        return stdString(_command.fontPostscriptName);
    }

    if(_command.fontFamily.empty() == false)
    {
        std::string key = stdString(_command.fontFamily);
        if(_command.fontStyle.empty() == false)
        {
            key += "-";
            key += stdString(_command.fontStyle);
        }

        return key;
    }

    return {};
}

//////////////////////////////////////////////////////////////////////////
std::string FreeTypeTextRenderer::normalizedFontName(std::string _value)
{
    _value.erase(std::remove_if(_value.begin(), _value.end(), [](char _ch) {
        return _ch == ' ' || _ch == '-';
    }), _value.end());
    std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char _ch) {
        return static_cast<char>(std::tolower(_ch));
    });
    return _value;
}

//////////////////////////////////////////////////////////////////////////
bool FreeTypeTextRenderer::faceMatches(FT_Face _face, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    const char * const postscriptName = FT_Get_Postscript_Name(_face);
    if(postscriptName != nullptr && _postscriptName.empty() == false && normalizedFontName(postscriptName) == normalizedFontName(_postscriptName))
    {
        return true;
    }

    if(_face->family_name != nullptr && _familyName.empty() == false && normalizedFontName(_face->family_name) == normalizedFontName(_familyName))
    {
        if(_styleName.empty() == true || _face->style_name == nullptr || normalizedFontName(_face->style_name) == normalizedFontName(_styleName))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
FT_Face FreeTypeTextRenderer::openFaceAtPath(NSString * _path, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    for(FT_Long faceIndex = 0; faceIndex != 32; ++faceIndex)
    {
        FT_Face face = nullptr;
        if(FT_New_Face(m_library, _path.fileSystemRepresentation, faceIndex, &face) != FT_Err_Ok)
        {
            return nullptr;
        }

        const FT_Long faceCount = face->num_faces;
        if(faceMatches(face, _postscriptName, _familyName, _styleName) == true)
        {
            if(FT_Select_Charmap(face, FT_ENCODING_UNICODE) != FT_Err_Ok)
            {
                FT_Done_Face(face);
                return nullptr;
            }

            return face;
        }

        FT_Done_Face(face);
        if(faceIndex + 1 >= faceCount)
        {
            break;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
FT_Face FreeTypeTextRenderer::openMatchingFaceInDirectory(NSString * _directory, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    NSFileManager * fileManager = [NSFileManager defaultManager];
    NSDirectoryEnumerator<NSString *> * files = [fileManager enumeratorAtPath:_directory];
    for(NSString * file in files)
    {
        NSString * extension = file.pathExtension.lowercaseString;
        if([extension isEqualToString:@"ttf"] == NO && [extension isEqualToString:@"ttc"] == NO && [extension isEqualToString:@"otf"] == NO)
        {
            continue;
        }

        NSString * path = [_directory stringByAppendingPathComponent:file];
        FT_Face face = this->openFaceAtPath(path, _postscriptName, _familyName, _styleName);
        if(face != nullptr)
        {
            return face;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
NSArray<NSString *> * FreeTypeTextRenderer::environmentFontSearchDirectories()
{
    NSMutableArray<NSString *> * directories = [NSMutableArray array];

    const char * const envValue = std::getenv("FIGMA_VIEWER_FONT_DIRS");
    if(envValue != nullptr && envValue[0] != '\0')
    {
        NSString * envString = [NSString stringWithUTF8String:envValue];
        if(envString != nil)
        {
            NSCharacterSet * trimSet = [NSCharacterSet whitespaceAndNewlineCharacterSet];
            NSArray<NSString *> * envDirectories = [envString componentsSeparatedByString:@":"];
            for(NSString * directory in envDirectories)
            {
                NSString * trimmed = [directory stringByTrimmingCharactersInSet:trimSet];
                if(trimmed.length == 0)
                {
                    continue;
                }

                [directories addObject:[trimmed stringByExpandingTildeInPath]];
            }
        }
    }

    return directories;
}

//////////////////////////////////////////////////////////////////////////
NSArray<NSString *> * FreeTypeTextRenderer::defaultFontSearchDirectories()
{
    return @[
        @"/System/Library/Fonts/Supplemental",
        @"/System/Library/Fonts",
        @"/Library/Fonts",
        [@"~/Library/Fonts" stringByExpandingTildeInPath],
    ];
}

//////////////////////////////////////////////////////////////////////////
NSString * FreeTypeTextRenderer::fontRequestDescription(const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    NSMutableArray<NSString *> * parts = [NSMutableArray array];
    if(_postscriptName.empty() == false)
    {
        [parts addObject:[NSString stringWithFormat:@"postscript=%@", [NSString stringWithUTF8String:_postscriptName.c_str()] ?: @""]];
    }

    if(_familyName.empty() == false)
    {
        [parts addObject:[NSString stringWithFormat:@"family=%@", [NSString stringWithUTF8String:_familyName.c_str()] ?: @""]];
    }

    if(_styleName.empty() == false)
    {
        [parts addObject:[NSString stringWithFormat:@"style=%@", [NSString stringWithUTF8String:_styleName.c_str()] ?: @""]];
    }

    return parts.count != 0 ? [parts componentsJoinedByString:@", "] : @"<empty>";
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::recordMissingFont(const ViewerRenderCommand & _command, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    const std::string key = this->fontKeyForCommand(_command);
    if(key.empty() == true)
    {
        return;
    }

    NSString * description = fontRequestDescription(_postscriptName, _familyName, _styleName);
    const std::string descriptionString = stdString(description);
    if(m_missingFonts.emplace(key, descriptionString).second == false)
    {
        return;
    }

    NSLog(@"Figma Viewer missing font for node %@: %@. Install the font in a system font directory, choose a font folder in the viewer, or pass a font collection with FIGMA_VIEWER_FONT_DIRS. Search directories: %@",
          nsString(_command.nodeId),
          description,
          [this->fontSearchDirectories() componentsJoinedByString:@", "]);
}

//////////////////////////////////////////////////////////////////////////
FT_Face FreeTypeTextRenderer::openConfiguredOrSystemFace(const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName)
{
    NSFileManager * fileManager = [NSFileManager defaultManager];

    for(NSString * directory in this->fontSearchDirectories())
    {
        BOOL isDirectory = NO;
        if([fileManager fileExistsAtPath:directory isDirectory:&isDirectory] == NO || isDirectory == NO)
        {
            continue;
        }

        FT_Face face = this->openMatchingFaceInDirectory(directory, _postscriptName, _familyName, _styleName);
        if(face != nullptr)
        {
            return face;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
FT_Face FreeTypeTextRenderer::openFaceForCommand(const ViewerRenderCommand & _command)
{
    const std::string postscriptName = stdString(_command.fontPostscriptName);
    const std::string familyName = stdString(_command.fontFamily);
    const std::string styleName = stdString(_command.fontStyle);

    if(postscriptName.empty() == true && familyName.empty() == true)
    {
        return nullptr;
    }

    FT_Face face = this->openConfiguredOrSystemFace(postscriptName, familyName, styleName);
    if(face != nullptr)
    {
        return face;
    }

    this->recordMissingFont(_command, postscriptName, familyName, styleName);
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::drawDecodedLines(FT_Face _face, const ViewerRenderCommand & _command, NSRect _rect, CGFloat _rasterScale)
{
    const std::size_t textLineSize = _command.textLines.size();
    for(std::size_t lineIndex = 0; lineIndex != textLineSize; ++lineIndex)
    {
        const ViewerRenderTextLineDesc & line = _command.textLines[lineIndex];
        std::string_view lineText = textLineView(_command, lineIndex);
        std::vector<char32_t> codepoints = decodeUtf8(lineText);
        if(codepoints.empty() == true)
        {
            continue;
        }

        const CGFloat baselineX = _rect.origin.x + static_cast<CGFloat>(line.x);
        const CGFloat ascent = line.lineAscent > 0.0f ? static_cast<CGFloat>(line.lineAscent) : faceAscender(_face, _rasterScale);
        const CGFloat baselineY = _rect.origin.y + static_cast<CGFloat>(line.y) + ascent;
        const CGFloat horizontalScale = horizontalScaleForLine(_face, codepoints, static_cast<CGFloat>(line.width) * _rasterScale);
        const CGFloat verticalScale = verticalScaleForLine(_face, _command, line, _rasterScale);
        this->drawLine(_face, _command, codepoints, baselineX, baselineY, _rasterScale, horizontalScale, verticalScale);
    }
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::drawLine(FT_Face _face, const ViewerRenderCommand & _command, const std::vector<char32_t> & _codepoints, CGFloat _baselineX, CGFloat _baselineY, CGFloat _rasterScale, CGFloat _horizontalScale, CGFloat _verticalScale)
{
    CGFloat penX = _baselineX;
    FT_UInt previousGlyph = 0;
    const CGFloat rasterScale = std::max<CGFloat>(1.0, _rasterScale);
    const CGFloat horizontalScale = std::max<CGFloat>(0.001, _horizontalScale);
    const CGFloat verticalScale = std::max<CGFloat>(0.001, _verticalScale);

    for(char32_t codepoint : _codepoints)
    {
        if(codepoint == U'\n' || codepoint == U'\r')
        {
            continue;
        }

        const FT_UInt glyphIndex = glyphIndexForCodepoint(_face, codepoint);

        if(previousGlyph != 0 && glyphIndex != 0 && FT_HAS_KERNING(_face) != 0)
        {
            FT_Vector kerning;
            if(FT_Get_Kerning(_face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &kerning) == FT_Err_Ok)
            {
                penX += to26Dot6(kerning.x) / rasterScale;
            }
        }

        if(FT_Load_Glyph(_face, glyphIndex, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT | FT_LOAD_COLOR) != FT_Err_Ok)
        {
            previousGlyph = glyphIndex;
            continue;
        }

        FT_GlyphSlot glyph = _face->glyph;
        if(glyph->bitmap.width > 0 && glyph->bitmap.rows > 0)
        {
            NSImage * glyphImage = makeGlyphImage(glyph->bitmap, _command);
            if(glyphImage != nil)
            {
                const CGFloat glyphX = _baselineX + (penX + static_cast<CGFloat>(glyph->bitmap_left) / rasterScale - _baselineX) * horizontalScale;
                const CGFloat glyphY = _baselineY - static_cast<CGFloat>(glyph->bitmap_top) / rasterScale * verticalScale;
                const NSRect glyphRect = NSMakeRect(glyphX,
                                                    glyphY,
                                                    static_cast<CGFloat>(glyph->bitmap.width) / rasterScale * horizontalScale,
                                                    static_cast<CGFloat>(glyph->bitmap.rows) / rasterScale * verticalScale);
                [glyphImage drawInRect:glyphRect fromRect:NSZeroRect operation:compositingOperationForCommand(_command) fraction:1.0 respectFlipped:YES hints:nil];
            }
        }

        penX += to26Dot6(glyph->metrics.horiAdvance) / rasterScale;
        previousGlyph = glyphIndex;
    }
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::rasterizeDecodedLines(FT_Face _face, const ViewerRenderCommand & _command, CGFloat _rasterScale, unsigned char * const _pixels, NSUInteger _width, NSUInteger _height, NSUInteger _stride)
{
    const std::size_t textLineSize = _command.textLines.size();
    for(std::size_t lineIndex = 0; lineIndex != textLineSize; ++lineIndex)
    {
        const ViewerRenderTextLineDesc & line = _command.textLines[lineIndex];
        std::string_view lineText = textLineView(_command, lineIndex);
        std::vector<char32_t> codepoints = decodeUtf8(lineText);
        if(codepoints.empty() == true)
        {
            continue;
        }

        const CGFloat ascent = line.lineAscent > 0.0f
            ? static_cast<CGFloat>(line.lineAscent) * _rasterScale
            : to26Dot6(_face->size->metrics.ascender);
        const CGFloat baselineX = static_cast<CGFloat>(line.x) * _rasterScale;
        const CGFloat baselineY = static_cast<CGFloat>(line.y) * _rasterScale + ascent;
        const CGFloat horizontalScale = horizontalScaleForLine(_face, codepoints, static_cast<CGFloat>(line.width) * _rasterScale);
        const CGFloat verticalScale = verticalScaleForLine(_face, _command, line, _rasterScale);
        this->rasterizeLine(_face, _command, codepoints, baselineX, baselineY, horizontalScale, verticalScale, _pixels, _width, _height, _stride);
    }
}

//////////////////////////////////////////////////////////////////////////
void FreeTypeTextRenderer::rasterizeLine(FT_Face _face, const ViewerRenderCommand & _command, const std::vector<char32_t> & _codepoints, CGFloat _baselineX, CGFloat _baselineY, CGFloat _horizontalScale, CGFloat _verticalScale, unsigned char * const _pixels, NSUInteger _width, NSUInteger _height, NSUInteger _stride)
{
    CGFloat penX = _baselineX;
    FT_UInt previousGlyph = 0;
    const CGFloat horizontalScale = std::max<CGFloat>(0.001, _horizontalScale);
    const CGFloat verticalScale = std::max<CGFloat>(0.001, _verticalScale);

    for(char32_t codepoint : _codepoints)
    {
        if(codepoint == U'\n' || codepoint == U'\r')
        {
            continue;
        }

        const FT_UInt glyphIndex = glyphIndexForCodepoint(_face, codepoint);

        if(previousGlyph != 0 && glyphIndex != 0 && FT_HAS_KERNING(_face) != 0)
        {
            FT_Vector kerning;
            if(FT_Get_Kerning(_face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &kerning) == FT_Err_Ok)
            {
                penX += to26Dot6(kerning.x);
            }
        }

        if(FT_Load_Glyph(_face, glyphIndex, FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT | FT_LOAD_COLOR) != FT_Err_Ok)
        {
            previousGlyph = glyphIndex;
            continue;
        }

        FT_GlyphSlot glyph = _face->glyph;
        const FT_Bitmap & bitmap = glyph->bitmap;
        if(bitmap.width != 0 && bitmap.rows != 0 && bitmap.buffer != nullptr)
        {
            const CGFloat glyphX = _baselineX + (penX + to26Dot6(glyph->metrics.horiBearingX) - _baselineX) * horizontalScale;
            const CGFloat glyphY = _baselineY - to26Dot6(glyph->metrics.horiBearingY) * verticalScale;
            const NSInteger destinationX = static_cast<NSInteger>(std::lround(glyphX));
            const NSInteger destinationY = static_cast<NSInteger>(std::lround(glyphY));
            const NSInteger destinationWidth = std::max<NSInteger>(1, static_cast<NSInteger>(std::lround(static_cast<CGFloat>(bitmap.width) * horizontalScale)));
            const NSInteger destinationHeight = std::max<NSInteger>(1, static_cast<NSInteger>(std::lround(static_cast<CGFloat>(bitmap.rows) * verticalScale)));

            for(NSInteger scaledY = 0; scaledY != destinationHeight; ++scaledY)
            {
                const NSInteger sourceY = std::min<NSInteger>(static_cast<NSInteger>(bitmap.rows) - 1, std::max<NSInteger>(0, static_cast<NSInteger>(std::floor((static_cast<CGFloat>(scaledY) + 0.5) / verticalScale))));
                const NSInteger targetY = destinationY + scaledY;
                if(targetY < 0 || targetY >= static_cast<NSInteger>(_height))
                {
                    continue;
                }

                unsigned char * targetRow = _pixels + static_cast<NSUInteger>(targetY) * _stride;
                for(NSInteger scaledX = 0; scaledX != destinationWidth; ++scaledX)
                {
                    const NSInteger sourceX = std::min<NSInteger>(static_cast<NSInteger>(bitmap.width) - 1, std::max<NSInteger>(0, static_cast<NSInteger>(std::floor((static_cast<CGFloat>(scaledX) + 0.5) / horizontalScale))));
                    const NSInteger targetX = destinationX + scaledX;
                    if(targetX < 0 || targetX >= static_cast<NSInteger>(_width))
                    {
                        continue;
                    }

                    blendGlyphBitmapPixel(bitmap, sourceX, sourceY, targetRow + static_cast<NSUInteger>(targetX) * 4, _command);
                }
            }
        }

        penX += to26Dot6(glyph->metrics.horiAdvance);
        previousGlyph = glyphIndex;
    }
}
