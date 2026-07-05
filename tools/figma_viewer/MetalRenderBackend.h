#pragma once

#include "FreeTypeTextRenderer.h"

#include "Figma/Figma.h"
#include "../../sdk/src/RenderList.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstdint>
#include <vector>

struct MetalVertexDesc
{
    float position[2];
    float uv[2];
    float color[4];
};

struct MetalUniformDesc
{
    float viewportSize[2];
    float targetSize[2];
    float commandRect[4];
    float opacity;
    std::uint32_t hasTexture;
    std::uint32_t shape;
    std::uint32_t blendMode;
    std::uint32_t pad0;
    float pad1[3];
};

static_assert(sizeof(MetalUniformDesc) == 64, "Metal uniform layout must stay 16-byte aligned");

class MetalRenderBackend
{
public:
    MetalRenderBackend();

    id<MTLDevice> device() const;
    bool isValid() const;
    void clearTextureCache();
    void render(CAMetalLayer * _layer,
                Figma::DocumentInterface * _document,
                FreeTypeTextRenderer * _textRenderer,
                const Figma::RenderListInterface * const _renderList,
                const std::vector<std::uint8_t> & _visibility,
                CGFloat _viewportWidth,
                CGFloat _viewportHeight,
                CGFloat _contentsScale);

protected:
    struct MipPixelsDesc
    {
        NSUInteger width = 0;
        NSUInteger height = 0;
        std::vector<std::uint8_t> pixels;
    };

    id<MTLRenderPipelineState> makePipeline(NSString * _vertexName, NSString * _fragmentName, MTLPixelFormat _pixelFormat);
    id<MTLTexture> makeWhiteTexture();
    id<MTLTexture> makeRenderTexture(NSUInteger _width, NSUInteger _height);
    void clearTexture(id<MTLCommandBuffer> _commandBuffer, id<MTLTexture> _texture, MTLClearColor _color);
    void copyTextureToTexture(id<MTLCommandBuffer> _commandBuffer, id<MTLTexture> _source, id<MTLTexture> _destination);
    static MipPixelsDesc makeNextMipLevel(const MipPixelsDesc & _source);
    static void premultiplyPixels(std::vector<std::uint8_t> * const _pixels);
    static std::vector<MipPixelsDesc> buildPremultipliedMipChain(std::vector<std::uint8_t> _pixels, NSUInteger _width, NSUInteger _height);
    id<MTLTexture> textureFromMipChain(std::vector<MipPixelsDesc> _levels);
    static void bleedTransparentPixels(std::vector<std::uint8_t> * const _pixels, NSUInteger _width, NSUInteger _height);
    static void fillTransparentPixelsWithAverageColor(std::vector<std::uint8_t> * const _pixels);
    id<MTLTexture> textureFromImage(NSImage * _image, NSString * _cacheKey);
    id<MTLTexture> textureFromImagePixels(std::vector<std::uint8_t> _pixels, NSUInteger _width, NSUInteger _height, NSString * _cacheKey);
    id<MTLTexture> textureFromRgbaPixels(const std::vector<std::uint8_t> & _pixels, NSUInteger _width, NSUInteger _height);
    id<MTLTexture> textureForCommand(Figma::DocumentInterface * _document, const Figma::RenderCommand & _command);
    id<MTLTexture> textureForText(FreeTypeTextRenderer * _textRenderer, const Figma::RenderCommand & _command, CGFloat _pixelScale);
    static void appendQuadVertices(std::vector<MetalVertexDesc> * const _vertices, const Figma::RenderCommand & _command);
    bool buildCommandGeometry(const Figma::RenderCommand & _command, std::vector<MetalVertexDesc> * const _vertices, std::vector<std::uint16_t> * const _indices);
    void drawVertices(id<MTLCommandBuffer> _commandBuffer,
                      id<MTLTexture> _target,
                      id<MTLTexture> _backdrop,
                      id<MTLTexture> _sourceTexture,
                      const std::vector<MetalVertexDesc> & _vertices,
                      const std::vector<std::uint16_t> & _indices,
                      const MetalUniformDesc & _uniforms);
    MetalUniformDesc makeUniforms(const Figma::RenderCommand & _command,
                                  bool _hasTexture,
                                  CGFloat _viewportWidth,
                                  CGFloat _viewportHeight,
                                  NSUInteger _targetWidth,
                                  NSUInteger _targetHeight) const;
    bool shouldSkipCommand(const Figma::RenderCommand & _command) const;
    void drawCommand(id<MTLCommandBuffer> _commandBuffer,
                     Figma::DocumentInterface * _document,
                     FreeTypeTextRenderer * _textRenderer,
                     const Figma::RenderCommand & _command,
                     id<MTLTexture> _target,
                     id<MTLTexture> _backdrop,
                     CGFloat _viewportWidth,
                     CGFloat _viewportHeight,
                     NSUInteger _targetWidth,
                     NSUInteger _targetHeight);
    void drawLayerTexture(id<MTLCommandBuffer> _commandBuffer,
                          id<MTLTexture> _layerTexture,
                          id<MTLTexture> _target,
                          id<MTLTexture> _backdrop,
                          CGFloat _opacity,
                          CGFloat _viewportWidth,
                          CGFloat _viewportHeight,
                          NSUInteger _targetWidth,
                          NSUInteger _targetHeight);
    void renderCommandRange(id<MTLCommandBuffer> _commandBuffer,
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
                            bool _allowLayers);
    void copyTexture(id<MTLCommandBuffer> _commandBuffer,
                     id<MTLTexture> _source,
                     id<MTLTexture> _destination,
                     CGFloat _viewportWidth,
                     CGFloat _viewportHeight,
                     NSUInteger _targetWidth,
                     NSUInteger _targetHeight);

protected:
    id<MTLDevice> m_device = nil;
    id<MTLCommandQueue> m_commandQueue = nil;
    id<MTLLibrary> m_library = nil;
    id<MTLRenderPipelineState> m_commandPipeline = nil;
    id<MTLRenderPipelineState> m_copyPipeline = nil;
    id<MTLSamplerState> m_sampler = nil;
    id<MTLTexture> m_whiteTexture = nil;
    NSMutableDictionary<NSString *, id<MTLTexture>> * m_textureCache = nil;
};
