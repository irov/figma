#pragma once

#include "Figma/Figma.h"

#include "../../sdk/src/Document.h"
#include "../../sdk/src/RenderList.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

template<class T>
struct FigmaInterfaceDeleter
{
    void operator()(T * const _ptr) const
    {
        if(_ptr != nullptr)
        {
            _ptr->destroy();
        }
    }
};

template<class T>
using FigmaInterfaceOwner = std::unique_ptr<T, FigmaInterfaceDeleter<T>>;

template<class T>
void destroyFigmaInterface(T *& _ptr)
{
    if(_ptr != nullptr)
    {
        _ptr->destroy();
        _ptr = nullptr;
    }
}

enum class EViewerWireframeMode
{
    Normal,
    Wireframe,
    Combined
};

struct PlaybackInputRecord
{
    NSTimeInterval time = 0.0;
    Figma::PointerEvent event;
};

inline constexpr CGFloat FigmaViewerInspectorWidth = 440.0;
inline constexpr CGFloat FigmaViewerInspectorRowHeight = 24.0;
inline constexpr CGFloat FigmaViewerInspectorDetailsLineHeight = 14.0;
inline constexpr CGFloat FigmaViewerInspectorDetailsVerticalPadding = 8.0;
inline constexpr CGFloat FigmaViewerInspectorHeaderHeight = 46.0;
inline constexpr NSTimeInterval FigmaViewerActiveFrameInterval = 1.0 / 30.0;
inline constexpr NSTimeInterval FigmaViewerIdleTimerInterval = 0.1;

const Figma::RenderCommandVector & privateRenderCommands(const Figma::RenderListInterface * const _renderList);
const Figma::Document * privateDocument(const Figma::DocumentInterface * _document);
const char * resultToString(Figma::EResult _result);
NSColor * colorFromCommand(const Figma::RenderCommand & _command, CGFloat _alpha);
NSColor * colorFromVertex(const Figma::RenderVertex & _vertex);
CGBlendMode cgBlendModeForCommand(const Figma::RenderCommand & _command);
NSCompositingOperation compositingOperationForCommand(const Figma::RenderCommand & _command);
const char * renderCommandTypeName(Figma::ERenderCommandType _type);
const char * renderShapeTypeName(Figma::ERenderShapeType _type);
const char * renderTextAlignHorizontalName(Figma::ERenderTextAlignHorizontal _type);
const char * renderTextAlignVerticalName(Figma::ERenderTextAlignVertical _type);
const char * renderBlendModeName(Figma::ERenderBlendMode _type);
const char * renderImageScaleModeName(Figma::ERenderImageScaleMode _type);
NSString * wireframeModeTitle(EViewerWireframeMode _mode);
NSString * nsString(const Figma::FigmaString & _value);
NSString * escapedInspectorString(const Figma::FigmaString & _value);
NSString * inspectorBoolString(bool _value);
NSString * inspectorRectString(const Figma::Rectf & _rect);
NSString * inspectorColorString(const Figma::Colorf & _color);
NSString * inspectorFloatArrayString(const float * _values, std::size_t _count);
std::string resolveFigPath(const char * _requestedPath);
float prototypeIntroAdvanceTime(const Figma::DocumentInterface * _document);
Figma::PlayerDesc makePlayerDesc(const Figma::DocumentInterface * _document);
Figma::EResult loadViewerDocument(Figma::RuntimeInterface * const _runtime,
                                  const std::string & _figPath,
                                  const char * _sidecarPath,
                                  Figma::DocumentInterface ** const _document,
                                  Figma::PlayerInterface ** const _player,
                                  Figma::PlayerDesc * const _playerDesc);
NSInteger playbackSpeedValueCount();
CGFloat playbackSpeedAtIndex(NSInteger _index);
NSInteger defaultPlaybackSpeedIndex();
NSString * playbackSpeedLabel(CGFloat _speed);
Figma::EPointerButton pointerButtonFromEvent(NSEvent * _event);
Figma::InputModifierFlags inputModifiersFromEvent(NSEvent * _event);
CGRect cgRectFromNSRect(NSRect _rect);
void addShapePath(CGContextRef _context, Figma::ERenderShapeType _shape, NSRect _rect, CGFloat _radius);
void drawImageCommand(NSImage * _image, const Figma::AssetDesc * _asset, const Figma::RenderCommand & _command, NSRect _rect);
void drawMeshCommand(const Figma::RenderCommand & _command);
