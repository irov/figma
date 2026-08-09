#pragma once

#include "ViewerRenderTypes.h"

#import <AppKit/AppKit.h>

#include "ft2build.h"
#include FT_FREETYPE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class FreeTypeTextRenderer
{
public:
    FreeTypeTextRenderer();
    ~FreeTypeTextRenderer();

    void drawText(const ViewerRenderCommand & _command, NSRect _rect);
    bool makeTextPixels(const ViewerRenderCommand & _command, NSSize _pointSize, CGFloat _rasterScale, std::vector<std::uint8_t> * const _pixels, NSUInteger * const _width, NSUInteger * const _height);
    void addFontSearchDirectory(NSString * _directory);
    void clearFontCache();
    void clearMissingFonts();
    void collectMissingFonts(const ViewerRenderCommandVector & _commands);
    NSArray<NSString *> * missingFontDescriptions() const;
    NSArray<NSString *> * fontSearchDirectories() const;

protected:
    void drawTextAtRasterScale(const ViewerRenderCommand & _command, NSRect _rect, CGFloat _rasterScale);
    static std::vector<char32_t> decodeUtf8(std::string_view _text);
    static CGFloat to26Dot6(FT_Pos _value);
    static FT_UInt glyphIndexForCodepoint(FT_Face _face, char32_t _codepoint);
    static CGFloat measureLineAdvance(FT_Face _face, const std::vector<char32_t> & _codepoints);
    static CGFloat horizontalScaleForLine(FT_Face _face, const std::vector<char32_t> & _codepoints, CGFloat _targetWidth);
    static CGFloat verticalScaleForLine(FT_Face _face, const ViewerRenderCommand & _command, const ViewerRenderTextLineDesc & _line, CGFloat _rasterScale);
    static std::string_view trimTrailingWhitespace(std::string_view _view);
    static std::string_view explicitLineSegment(std::string_view _view, std::size_t _lineIndex);
    static std::string_view sourceTextView(const ViewerRenderCommand & _command, const ViewerRenderTextLineDesc & _line);
    static std::string_view textLineView(const ViewerRenderCommand & _command, std::size_t _lineIndex);
    static CGFloat faceAscender(FT_Face _face, CGFloat _rasterScale);
    static std::uint8_t coverageAt(const FT_Bitmap & _bitmap, NSInteger _x, NSInteger _y);
    static void blendStraightPixel(unsigned char * const _target, CGFloat _red, CGFloat _green, CGFloat _blue, CGFloat _alpha);
    static void blendGlyphBitmapPixel(const FT_Bitmap & _bitmap, NSInteger _sourceX, NSInteger _sourceY, unsigned char * const _target, const ViewerRenderCommand & _command);
    static NSImage * makeGlyphImage(const FT_Bitmap & _bitmap, const ViewerRenderCommand & _command);
    FT_Face faceForCommand(const ViewerRenderCommand & _command);
    std::string fontKeyForCommand(const ViewerRenderCommand & _command) const;
    static std::string normalizedFontName(std::string _value);
    static bool faceMatches(FT_Face _face, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    FT_Face openFaceAtPath(NSString * _path, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    FT_Face openMatchingFaceInDirectory(NSString * _directory, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    static NSArray<NSString *> * defaultFontSearchDirectories();
    static NSString * fontRequestDescription(const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    void recordMissingFont(const ViewerRenderCommand & _command, const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    FT_Face openConfiguredOrSystemFace(const std::string & _postscriptName, const std::string & _familyName, const std::string & _styleName);
    FT_Face openFaceForCommand(const ViewerRenderCommand & _command);
    void drawDecodedLines(FT_Face _face, const ViewerRenderCommand & _command, NSRect _rect, CGFloat _rasterScale);
    void drawLine(FT_Face _face, const ViewerRenderCommand & _command, const std::vector<char32_t> & _codepoints, CGFloat _baselineX, CGFloat _baselineY, CGFloat _rasterScale, CGFloat _horizontalScale, CGFloat _verticalScale);
    void rasterizeDecodedLines(FT_Face _face, const ViewerRenderCommand & _command, CGFloat _rasterScale, unsigned char * const _pixels, NSUInteger _width, NSUInteger _height, NSUInteger _stride);
    void rasterizeLine(FT_Face _face, const ViewerRenderCommand & _command, const std::vector<char32_t> & _codepoints, CGFloat _baselineX, CGFloat _baselineY, CGFloat _horizontalScale, CGFloat _verticalScale, unsigned char * const _pixels, NSUInteger _width, NSUInteger _height, NSUInteger _stride);

protected:
    FT_Library m_library = nullptr;
    std::unordered_map<std::string, FT_Face> m_faces;
    std::unordered_map<std::string, std::string> m_missingFonts;
    std::vector<std::string> m_fontDirectories;
};
