#pragma once

#include "ViewerRenderTypes.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdint>
#include <string>
#include <vector>

enum class EViewerWireframeMode
{
    Normal,
    Wireframe,
    Combined
};

struct PlaybackInputRecord
{
    NSTimeInterval time = 0.0;
    figma_pointer_event_t event = {};
};

inline constexpr CGFloat FigmaViewerInspectorWidth = 440.0;
inline constexpr CGFloat FigmaViewerInspectorRowHeight = 24.0;
inline constexpr CGFloat FigmaViewerInspectorDetailsLineHeight = 14.0;
inline constexpr CGFloat FigmaViewerInspectorDetailsVerticalPadding = 8.0;
inline constexpr CGFloat FigmaViewerInspectorHeaderHeight = 46.0;
inline constexpr NSTimeInterval FigmaViewerActiveFrameInterval = 1.0 / 30.0;
inline constexpr NSTimeInterval FigmaViewerIdleTimerInterval = 0.1;

void destroyFigmaObject(figma_player_t *& player);
void destroyFigmaObject(figma_document_t *& document);
void destroyFigmaObject(figma_runtime_t *& runtime);
const char * resultToString(figma_result_t result);
NSColor * colorFromCommand(const ViewerRenderCommand & command, CGFloat alpha);
NSColor * colorFromVertex(const figma_render_vertex_t & vertex);
CGBlendMode cgBlendModeForCommand(const ViewerRenderCommand & command);
NSCompositingOperation compositingOperationForCommand(const ViewerRenderCommand & command);
const char * renderCommandTypeName(figma_render_command_type_t type);
const char * renderShapeTypeName(figma_render_shape_type_t type);
const char * renderTextAlignHorizontalName(figma_render_text_align_horizontal_t type);
const char * renderTextAlignVerticalName(figma_render_text_align_vertical_t type);
const char * renderBlendModeName(figma_render_blend_mode_t type);
const char * renderImageScaleModeName(figma_render_image_scale_mode_t type);
NSString * wireframeModeTitle(EViewerWireframeMode _mode);
NSString * nsString(const std::string & value);
NSString * nsString(figma_string_view_t value);
NSString * documentPath(const figma_document_t * document);
NSString * documentFileName(const figma_document_t * document);
NSString * escapedInspectorString(const std::string & value);
NSString * inspectorBoolString(bool _value);
NSString * inspectorRectString(const figma_rectf_t & rect);
NSString * inspectorColorString(const figma_colorf_t & color);
NSString * inspectorFloatArrayString(const float * _values, std::size_t _count);
std::string resolveFigPath(const char * _requestedPath);
float prototypeIntroAdvanceTime(const figma_document_t * document);
figma_player_desc_t makePlayerDesc(const figma_document_t * document);
figma_result_t loadViewerDocument(figma_runtime_t * runtime,
                                  const std::string & figPath,
                                  const char * sidecarPath,
                                  figma_document_t ** document,
                                  figma_player_t ** player,
                                  figma_player_desc_t * playerDesc);
NSInteger playbackSpeedValueCount();
CGFloat playbackSpeedAtIndex(NSInteger _index);
NSInteger defaultPlaybackSpeedIndex();
NSString * playbackSpeedLabel(CGFloat _speed);
figma_pointer_button_t pointerButtonFromEvent(NSEvent * event);
figma_input_modifier_flags_t inputModifiersFromEvent(NSEvent * event);
CGRect cgRectFromNSRect(NSRect _rect);
void addShapePath(CGContextRef context, figma_render_shape_type_t shape, NSRect rect, CGFloat radius);
void drawImageCommand(NSImage * image, const figma_asset_desc_t * asset, const ViewerRenderCommand & command, NSRect rect);
void drawMeshCommand(const ViewerRenderCommand & command);
