#include "FigmaViewerView.h"

#include "FreeTypeTextRenderer.h"
#include "MetalRenderBackend.h"

#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

@implementation FigmaViewerView
{
    std::unique_ptr<FreeTypeTextRenderer> m_textRenderer;
    std::unique_ptr<MetalRenderBackend> m_metalRenderer;
    CAMetalLayer * m_metalLayer;
    NSButton * m_restartButton;
    NSButton * m_playPauseButton;
    NSButton * m_stepBackButton;
    NSButton * m_stepForwardButton;
    NSButton * m_wireframeButton;
    NSButton * m_copyPropertiesButton;
    NSSlider * m_speedSlider;
    NSTextField * m_speedLabel;
    std::vector<PlaybackInputRecord> m_playbackInputs;
    std::vector<std::uint8_t> m_commandVisibility;
    std::vector<std::uint8_t> m_commandExpanded;
    NSTimeInterval m_playbackElapsed;
    NSSize m_cameraPan;
    NSPoint m_lastPanPoint;
    CGFloat m_commandListScroll;
    CGFloat m_cameraZoom;
    NSInteger m_selectedCommandIndex;
    EViewerWireframeMode m_wireframeMode;
    BOOL m_spaceDown;
    BOOL m_cameraPanning;
    BOOL m_replayingPlayback;
}

//////////////////////////////////////////////////////////////////////////
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if(self != nil)
    {
        m_cameraZoom = 1.0;
        m_cameraPan = NSMakeSize(0.0, 0.0);
        m_commandListScroll = 0.0;
        m_selectedCommandIndex = -1;
        m_wireframeMode = EViewerWireframeMode::Normal;
        m_spaceDown = NO;
        m_cameraPanning = NO;
        self.playbackPaused = NO;
        self.playbackSpeed = playbackSpeedAtIndex(defaultPlaybackSpeedIndex());
        m_playbackElapsed = 0.0;
        m_replayingPlayback = NO;
        self.wantsLayer = YES;
        self.layer.backgroundColor = [NSColor colorWithCalibratedRed:0.06 green:0.07 blue:0.08 alpha:1.0].CGColor;
        self.layer.geometryFlipped = YES;

        m_metalRenderer = std::make_unique<MetalRenderBackend>();
        if(m_metalRenderer->isValid() == true)
        {
            m_metalLayer = [CAMetalLayer layer];
            m_metalLayer.device = m_metalRenderer->device();
            m_metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            m_metalLayer.framebufferOnly = NO;
            m_metalLayer.opaque = YES;
            m_metalLayer.magnificationFilter = kCAFilterLinear;
            m_metalLayer.minificationFilter = kCAFilterTrilinear;
            m_metalLayer.geometryFlipped = YES;
            [self.layer addSublayer:m_metalLayer];
        }

        m_restartButton = [NSButton buttonWithTitle:@"Restart" target:self action:@selector(restartPlayback:)];
        m_restartButton.bezelStyle = NSBezelStyleRounded;
        m_restartButton.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
        [self addSubview:m_restartButton];

        m_playPauseButton = [NSButton buttonWithTitle:@"Pause" target:self action:@selector(togglePlayback:)];
        m_playPauseButton.bezelStyle = NSBezelStyleRounded;
        m_playPauseButton.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
        [self addSubview:m_playPauseButton];

        m_stepBackButton = [NSButton buttonWithTitle:@"-1f" target:self action:@selector(stepPlaybackBackward:)];
        m_stepBackButton.bezelStyle = NSBezelStyleRounded;
        m_stepBackButton.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
        m_stepBackButton.toolTip = @"Step one slowed frame backward";
        [self addSubview:m_stepBackButton];

        m_stepForwardButton = [NSButton buttonWithTitle:@"+1f" target:self action:@selector(stepPlaybackForward:)];
        m_stepForwardButton.bezelStyle = NSBezelStyleRounded;
        m_stepForwardButton.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
        m_stepForwardButton.toolTip = @"Step one slowed frame forward";
        [self addSubview:m_stepForwardButton];

        m_wireframeButton = [NSButton buttonWithTitle:wireframeModeTitle(m_wireframeMode) target:self action:@selector(toggleWireframeMode:)];
        m_wireframeButton.bezelStyle = NSBezelStyleRounded;
        m_wireframeButton.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
        m_wireframeButton.toolTip = @"Switch render mode";
        [self addSubview:m_wireframeButton];

        m_copyPropertiesButton = [NSButton buttonWithTitle:@"Copy Props" target:self action:@selector(copyCurrentCommandProperties:)];
        m_copyPropertiesButton.bezelStyle = NSBezelStyleRounded;
        m_copyPropertiesButton.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold];
        m_copyPropertiesButton.toolTip = @"Copy selected render command properties";
        m_copyPropertiesButton.hidden = YES;
        m_copyPropertiesButton.enabled = NO;
        [self addSubview:m_copyPropertiesButton];

        m_speedSlider = [NSSlider sliderWithValue:static_cast<double>(defaultPlaybackSpeedIndex())
                                         minValue:0.0
                                         maxValue:static_cast<double>(playbackSpeedValueCount() - 1)
                                           target:self
                                           action:@selector(changePlaybackSpeed:)];
        m_speedSlider.numberOfTickMarks = playbackSpeedValueCount();
        m_speedSlider.allowsTickMarkValuesOnly = YES;
        [self addSubview:m_speedSlider];

        m_speedLabel = [NSTextField labelWithString:playbackSpeedLabel(self.playbackSpeed)];
        m_speedLabel.font = [NSFont monospacedDigitSystemFontOfSize:12.0 weight:NSFontWeightSemibold];
        m_speedLabel.textColor = [NSColor colorWithCalibratedWhite:0.82 alpha:1.0];
        m_speedLabel.alignment = NSTextAlignmentLeft;
        [self addSubview:m_speedLabel];
    }

    return self;
}

//////////////////////////////////////////////////////////////////////////
- (FreeTypeTextRenderer *)textRenderer
{
    if(m_textRenderer == nullptr)
    {
        m_textRenderer = std::make_unique<FreeTypeTextRenderer>();
    }

    return m_textRenderer.get();
}

//////////////////////////////////////////////////////////////////////////
- (NSArray<NSString *> *)collectMissingFontDescriptions
{
    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr)
    {
        return @[];
    }

    FreeTypeTextRenderer * renderer = [self textRenderer];
    renderer->collectMissingFonts(*commands);
    return renderer->missingFontDescriptions();
}

//////////////////////////////////////////////////////////////////////////
- (NSArray<NSString *> *)fontSearchDirectories
{
    return [self textRenderer]->fontSearchDirectories();
}

//////////////////////////////////////////////////////////////////////////
- (void)addFontSearchDirectory:(NSString *)_directory
{
    [self textRenderer]->addFontSearchDirectory(_directory);

    if(m_metalRenderer != nullptr)
    {
        m_metalRenderer->clearTextureCache();
    }

    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)configureWithDocument:(Figma::DocumentInterface *)_newDocument player:(Figma::PlayerInterface *)_newPlayer viewportWidth:(CGFloat)_newViewportWidth viewportHeight:(CGFloat)_newViewportHeight
{
    self.document = _newDocument;
    self.player = _newPlayer;
    self.viewportWidth = _newViewportWidth;
    self.viewportHeight = _newViewportHeight;
    self.playbackPaused = NO;
    self.playbackSpeed = playbackSpeedAtIndex(defaultPlaybackSpeedIndex());

    m_playbackInputs.clear();
    m_commandVisibility.clear();
    m_commandExpanded.clear();
    m_playbackElapsed = 0.0;
    m_commandListScroll = 0.0;
    m_selectedCommandIndex = -1;
    m_cameraZoom = 1.0;
    m_cameraPan = NSMakeSize(0.0, 0.0);
    m_cameraPanning = NO;
    m_replayingPlayback = NO;
    m_textRenderer.reset();
    if(m_metalRenderer != nullptr)
    {
        m_metalRenderer->clearTextureCache();
    }

    if(m_speedSlider != nil)
    {
        m_speedSlider.integerValue = defaultPlaybackSpeedIndex();
    }
    [self updatePlaybackControlTitles];
    [self updateCopyPropertiesButtonState];
    [self notifyPlaybackControlsChanged];
    [self setNeedsLayout:YES];
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)isFlipped
{
    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)acceptsFirstResponder
{
    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)notifyPlaybackControlsChanged
{
    id delegate = NSApp.delegate;
    SEL selector = @selector(refreshTimerMode);
    if(delegate != nil && [delegate respondsToSelector:selector] == YES)
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
        [delegate performSelector:selector];
#pragma clang diagnostic pop
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)updatePlaybackControlTitles
{
    m_playPauseButton.title = self.playbackPaused == YES ? @"Run" : @"Pause";
    m_speedLabel.stringValue = playbackSpeedLabel(self.playbackSpeed);
}

//////////////////////////////////////////////////////////////////////////
- (NSTimeInterval)playbackStepDuration
{
    return (1.0 / 60.0) * std::max<CGFloat>(0.0, self.playbackSpeed);
}

//////////////////////////////////////////////////////////////////////////
- (void)resetPlaybackTimeline:(BOOL)_clearInputs paused:(BOOL)_paused
{
    if(self.player == nullptr)
    {
        return;
    }

    self.player->restart();
    const float advanceTime = prototypeIntroAdvanceTime(self.document);
    if(advanceTime > 0.0f)
    {
        self.player->update(advanceTime);
    }

    m_playbackElapsed = 0.0;
    if(_clearInputs == YES)
    {
        m_playbackInputs.clear();
    }

    self.playbackPaused = _paused;
    [self updatePlaybackControlTitles];
    [self notifyPlaybackControlsChanged];
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)recordPlaybackPointerEvent:(const Figma::PointerEvent &)_event
{
    if(m_replayingPlayback == YES)
    {
        return;
    }

    const NSTimeInterval playbackElapsed = m_playbackElapsed;
    auto eraseIt = std::remove_if(m_playbackInputs.begin(), m_playbackInputs.end(), [playbackElapsed](const PlaybackInputRecord & _record) {
        return _record.time > playbackElapsed + 0.000001;
    });
    m_playbackInputs.erase(eraseIt, m_playbackInputs.end());

    PlaybackInputRecord record;
    record.time = m_playbackElapsed;
    record.event = _event;
    m_playbackInputs.emplace_back(record);
}

//////////////////////////////////////////////////////////////////////////
- (void)seekPlaybackToElapsed:(NSTimeInterval)_targetElapsed
{
    if(self.player == nullptr)
    {
        return;
    }

    const NSTimeInterval targetElapsed = std::max<NSTimeInterval>(0.0, _targetElapsed);

    m_replayingPlayback = YES;
    self.player->restart();
    const float advanceTime = prototypeIntroAdvanceTime(self.document);
    if(advanceTime > 0.0f)
    {
        self.player->update(advanceTime);
    }

    NSTimeInterval replayElapsed = 0.0;
    for(const PlaybackInputRecord & record : m_playbackInputs)
    {
        if(record.time > targetElapsed + 0.000001)
        {
            break;
        }

        const NSTimeInterval eventDelta = std::max<NSTimeInterval>(0.0, record.time - replayElapsed);
        if(eventDelta > 0.0)
        {
            self.player->update(static_cast<float>(eventDelta));
            replayElapsed += eventDelta;
        }

        self.player->inputPointer(record.event);
        self.player->update(0.0f);
    }

    const NSTimeInterval tailDelta = std::max<NSTimeInterval>(0.0, targetElapsed - replayElapsed);
    if(tailDelta > 0.0)
    {
        self.player->update(static_cast<float>(tailDelta));
    }

    m_playbackElapsed = targetElapsed;
    m_replayingPlayback = NO;
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)advancePlaybackBy:(NSTimeInterval)_dt
{
    if(self.player == nullptr)
    {
        return;
    }

    const NSTimeInterval dt = std::max<NSTimeInterval>(0.0, _dt);
    if(dt <= 0.0)
    {
        self.player->update(0.0f);
        [self setNeedsDisplay:YES];
        return;
    }

    self.player->update(static_cast<float>(dt));
    m_playbackElapsed += dt;
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)layout
{
    [super layout];
    m_restartButton.frame = NSMakeRect(14.0, 14.0, 86.0, 28.0);
    m_playPauseButton.frame = NSMakeRect(108.0, 14.0, 74.0, 28.0);
    m_stepBackButton.frame = NSMakeRect(190.0, 14.0, 48.0, 28.0);
    m_stepForwardButton.frame = NSMakeRect(244.0, 14.0, 48.0, 28.0);
    m_speedSlider.frame = NSMakeRect(304.0, 16.0, 180.0, 24.0);
    m_speedLabel.frame = NSMakeRect(494.0, 18.0, 72.0, 18.0);
    m_wireframeButton.frame = NSMakeRect(574.0, 14.0, 96.0, 28.0);
    const NSRect inspector = [self inspectorRect];
    if(NSIsEmptyRect(inspector) == YES)
    {
        m_copyPropertiesButton.hidden = YES;
    }
    else
    {
        m_copyPropertiesButton.frame = NSMakeRect(NSMaxX(inspector) - 104.0, inspector.origin.y + 11.0, 88.0, 24.0);
        m_copyPropertiesButton.hidden = NO;
    }
    [self updateCopyPropertiesButtonState];
    [self updateMetalLayerFrame];
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)metalContentsScale
{
    if(self.window != nil)
    {
        return std::max<CGFloat>(1.0, self.window.backingScaleFactor);
    }

    return std::max<CGFloat>(1.0, NSScreen.mainScreen.backingScaleFactor);
}

//////////////////////////////////////////////////////////////////////////
- (void)updateMetalLayerFrame
{
    if(m_metalLayer == nil)
    {
        return;
    }

    const NSRect screen = [self screenRect];
    m_metalLayer.hidden = self.player == nullptr || NSIsEmptyRect(screen) == YES;
    m_metalLayer.frame = cgRectFromNSRect(screen);
    m_metalLayer.contentsScale = [self metalContentsScale];
    m_metalLayer.drawableSize = CGSizeMake(std::ceil(std::max<CGFloat>(1.0, screen.size.width * m_metalLayer.contentsScale)),
                                           std::ceil(std::max<CGFloat>(1.0, screen.size.height * m_metalLayer.contentsScale)));
}

//////////////////////////////////////////////////////////////////////////
- (void)renderMetalViewport
{
    if(self.player == nullptr || m_metalLayer == nil || m_metalRenderer == nullptr || m_metalRenderer->isValid() == false)
    {
        return;
    }

    [self syncCommandVisibility];
    [self updateMetalLayerFrame];
    m_metalRenderer->render(m_metalLayer,
                            self.document,
                            [self textRenderer],
                            self.player->getRenderList(),
                            m_commandVisibility,
                            self.viewportWidth,
                            self.viewportHeight,
                            [self metalContentsScale]);
}

//////////////////////////////////////////////////////////////////////////
- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    self.window.acceptsMouseMovedEvents = YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)updateTrackingAreas
{
    [super updateTrackingAreas];

    NSArray<NSTrackingArea *> * areas = [self.trackingAreas copy];
    for(NSTrackingArea * area in areas)
    {
        [self removeTrackingArea:area];
    }

    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect;
    NSTrackingArea * trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds options:options owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

//////////////////////////////////////////////////////////////////////////
- (void)keyDown:(NSEvent *)event
{
    if(event.keyCode == 49)
    {
        m_spaceDown = YES;
        return;
    }

    if(event.keyCode == 53)
    {
        m_cameraZoom = 1.0;
        m_cameraPan = NSMakeSize(0.0, 0.0);
        m_cameraPanning = NO;
        [self setNeedsDisplay:YES];
        return;
    }

    NSString * characters = event.charactersIgnoringModifiers;
    if([characters caseInsensitiveCompare:@"h"] == NSOrderedSame)
    {
        self.showHotspots = !self.showHotspots;
        [self setNeedsDisplay:YES];
        return;
    }

    if(self.player != nullptr)
    {
        Figma::KeyEvent key;
        key.type = Figma::EKeyEventType::Down;
        key.keyCode = static_cast<std::uint32_t>(event.keyCode);
        key.modifiers = inputModifiersFromEvent(event);
        self.player->inputKey(key);
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)keyUp:(NSEvent *)event
{
    if(event.keyCode == 49)
    {
        m_spaceDown = NO;
        m_cameraPanning = NO;
        return;
    }

    if(self.player != nullptr)
    {
        Figma::KeyEvent key;
        key.type = Figma::EKeyEventType::Up;
        key.keyCode = static_cast<std::uint32_t>(event.keyCode);
        key.modifiers = inputModifiersFromEvent(event);
        self.player->inputKey(key);
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)togglePlayback:(id)sender
{
    (void)sender;

    self.playbackPaused = !self.playbackPaused;
    [self updatePlaybackControlTitles];
    [self notifyPlaybackControlsChanged];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (void)toggleWireframeMode:(id)sender
{
    (void)sender;

    switch(m_wireframeMode)
    {
    case EViewerWireframeMode::Normal:
        m_wireframeMode = EViewerWireframeMode::Wireframe;
        break;
    case EViewerWireframeMode::Wireframe:
        m_wireframeMode = EViewerWireframeMode::Combined;
        break;
    case EViewerWireframeMode::Combined:
        m_wireframeMode = EViewerWireframeMode::Normal;
        break;
    }

    m_wireframeButton.title = wireframeModeTitle(m_wireframeMode);
    [self setNeedsDisplay:YES];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (void)changePlaybackSpeed:(id)sender
{
    (void)sender;

    const NSInteger index = std::max<NSInteger>(0, std::min<NSInteger>(static_cast<NSInteger>(std::llround(m_speedSlider.doubleValue)), playbackSpeedValueCount() - 1));
    m_speedSlider.integerValue = index;
    self.playbackSpeed = playbackSpeedAtIndex(index);
    [self updatePlaybackControlTitles];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (void)stepPlaybackBackward:(id)sender
{
    (void)sender;

    self.playbackPaused = YES;
    [self updatePlaybackControlTitles];
    [self notifyPlaybackControlsChanged];
    [self seekPlaybackToElapsed:m_playbackElapsed - [self playbackStepDuration]];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (void)stepPlaybackForward:(id)sender
{
    (void)sender;

    self.playbackPaused = YES;
    [self updatePlaybackControlTitles];
    [self notifyPlaybackControlsChanged];
    [self advancePlaybackBy:[self playbackStepDuration]];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (void)restartPlayback:(id)sender
{
    (void)sender;

    if(self.player != nullptr)
    {
        [self resetPlaybackTimeline:YES paused:YES];
    }

    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)diagnosticsWidth
{
    const Figma::DiagnosticsInterface * diagnostics = self.player != nullptr ? self.player->getDiagnostics() : nullptr;
    if(diagnostics == nullptr || diagnostics->getItems().empty() == true)
    {
        return 0.0;
    }

    return self.bounds.size.width >= 1180.0 ? 360.0 : 0.0;
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)inspectorWidth
{
    return self.bounds.size.width >= 980.0 ? FigmaViewerInspectorWidth : 0.0;
}

//////////////////////////////////////////////////////////////////////////
- (NSRect)inspectorRect
{
    const CGFloat width = [self inspectorWidth];
    if(width <= 0.0)
    {
        return NSZeroRect;
    }

    return NSMakeRect(self.bounds.size.width - width, 0.0, width, self.bounds.size.height);
}

//////////////////////////////////////////////////////////////////////////
- (const Figma::RenderCommandVector *)renderCommands
{
    if(self.player == nullptr)
    {
        return nullptr;
    }

    const Figma::RenderListInterface * renderList = self.player->getRenderList();
    return renderList != nullptr ? &privateRenderCommands(renderList) : nullptr;
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)hasSelectedCommand
{
    const Figma::RenderCommandVector * commands = [self renderCommands];
    return commands != nullptr && m_selectedCommandIndex >= 0 && static_cast<std::size_t>(m_selectedCommandIndex) < commands->size() ? YES : NO;
}

//////////////////////////////////////////////////////////////////////////
- (void)updateCopyPropertiesButtonState
{
    if(m_copyPropertiesButton == nil)
    {
        return;
    }

    const BOOL inspectorVisible = NSIsEmptyRect([self inspectorRect]) == NO;
    m_copyPropertiesButton.hidden = inspectorVisible == YES ? NO : YES;
    m_copyPropertiesButton.enabled = inspectorVisible == YES && [self hasSelectedCommand] == YES ? YES : NO;
}

//////////////////////////////////////////////////////////////////////////
- (void)syncCommandVisibility
{
    const Figma::RenderCommandVector * commands = [self renderCommands];
    const std::size_t commandCount = commands != nullptr ? commands->size() : 0;
    if(m_commandVisibility.size() != commandCount)
    {
        m_commandVisibility.resize(commandCount, 1);
    }

    if(m_commandExpanded.size() != commandCount)
    {
        m_commandExpanded.resize(commandCount, 0);
    }

    if(m_selectedCommandIndex >= 0 && static_cast<std::size_t>(m_selectedCommandIndex) >= commandCount)
    {
        m_selectedCommandIndex = -1;
    }

    [self updateCopyPropertiesButtonState];
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)isCommandVisibleAtIndex:(std::size_t)_index
{
    [self syncCommandVisibility];
    if(_index >= m_commandVisibility.size())
    {
        return YES;
    }

    return m_commandVisibility[_index] != 0 ? YES : NO;
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)isCommandExpandedAtIndex:(std::size_t)_index
{
    [self syncCommandVisibility];
    if(_index >= m_commandExpanded.size())
    {
        return NO;
    }

    return m_commandExpanded[_index] != 0 ? YES : NO;
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)inspectorDiagnosticsHeight
{
    if(self.player == nullptr)
    {
        return 0.0;
    }

    const Figma::DiagnosticsInterface * diagnostics = self.player != nullptr ? self.player->getDiagnostics() : nullptr;
    return diagnostics != nullptr && diagnostics->getItems().empty() == false ? 106.0 : 0.0;
}

//////////////////////////////////////////////////////////////////////////
- (NSRect)inspectorListRectForInspectorRect:(NSRect)_rect
{
    const CGFloat diagnosticsHeight = [self inspectorDiagnosticsHeight];
    return NSMakeRect(_rect.origin.x,
                      _rect.origin.y + FigmaViewerInspectorHeaderHeight,
                      _rect.size.width,
                      std::max<CGFloat>(1.0, _rect.size.height - FigmaViewerInspectorHeaderHeight - diagnosticsHeight));
}

//////////////////////////////////////////////////////////////////////////
- (NSArray<NSString *> *)detailLinesForCommand:(const Figma::RenderCommand *)_command index:(std::size_t)_index
{
    NSMutableArray<NSString *> * lines = [NSMutableArray array];
    if(_command == nullptr)
    {
        return lines;
    }

    const Figma::RenderCommand & command = *_command;
    [lines addObject:[NSString stringWithFormat:@"document.path: %@", self.document != nullptr ? nsString(privateDocument(self.document)->getPath()) : @""]];
    [lines addObject:[NSString stringWithFormat:@"document.fileName: %@", self.document != nullptr ? nsString(privateDocument(self.document)->getFileName()) : @""]];
    [lines addObject:[NSString stringWithFormat:@"command.index: %zu", _index]];
    [lines addObject:[NSString stringWithFormat:@"command.type: %s", renderCommandTypeName(command.type)]];
    [lines addObject:[NSString stringWithFormat:@"command.id: %@", nsString(command.id)]];
    [lines addObject:[NSString stringWithFormat:@"command.nodeId: %@", nsString(command.nodeId)]];
    [lines addObject:[NSString stringWithFormat:@"command.assetId: %@", nsString(command.assetId)]];
    [lines addObject:[NSString stringWithFormat:@"rect: %@", inspectorRectString(command.rect)]];
    [lines addObject:[NSString stringWithFormat:@"color: %@", inspectorColorString(command.color)]];
    [lines addObject:[NSString stringWithFormat:@"opacity: %.4f", command.opacity]];
    [lines addObject:[NSString stringWithFormat:@"blendMode: %s", renderBlendModeName(command.blendMode)]];
    [lines addObject:[NSString stringWithFormat:@"renderLayer: id=%u opacity=%.4f", command.renderLayerId, command.renderLayerOpacity]];
    [lines addObject:[NSString stringWithFormat:@"shape: %s cornerRadius=%.4f strokeWidth=%.4f", renderShapeTypeName(command.shape), command.cornerRadius, command.strokeWidth]];
    [lines addObject:[NSString stringWithFormat:@"arc: has=%@ start=%.4f end=%.4f innerRadius=%.4f", inspectorBoolString(command.hasArcDataValue), command.arcStartingAngle, command.arcEndingAngle, command.arcInnerRadius]];
    [lines addObject:[NSString stringWithFormat:@"image.scaleMode: %s originalSize=%ux%u", renderImageScaleModeName(command.imageScaleMode), command.originalImageWidth, command.originalImageHeight]];
    [lines addObject:[NSString stringWithFormat:@"imageTransform.has: %@", inspectorBoolString(command.hasImageTransformValue)]];
    [lines addObject:[NSString stringWithFormat:@"imageTransform: %@", inspectorFloatArrayString(command.imageTransform, std::size(command.imageTransform))]];
    [lines addObject:[NSString stringWithFormat:@"filterColorAdjust.has: %@", inspectorBoolString(command.hasFilterColorAdjustValue)]];
    [lines addObject:[NSString stringWithFormat:@"filterColorAdjust: %@", inspectorFloatArrayString(command.filterColorAdjust, std::size(command.filterColorAdjust))]];
    [lines addObject:[NSString stringWithFormat:@"paintFilter.has: %@", inspectorBoolString(command.hasPaintFilterValue)]];
    [lines addObject:[NSString stringWithFormat:@"paintFilter: %@", inspectorFloatArrayString(command.paintFilter, std::size(command.paintFilter))]];
    [lines addObject:[NSString stringWithFormat:@"text.value: %@", escapedInspectorString(command.text)]];
    [lines addObject:[NSString stringWithFormat:@"text.fontFamily: %@", nsString(command.fontFamily)]];
    [lines addObject:[NSString stringWithFormat:@"text.fontStyle: %@", nsString(command.fontStyle)]];
    [lines addObject:[NSString stringWithFormat:@"text.fontPostscriptName: %@", nsString(command.fontPostscriptName)]];
    [lines addObject:[NSString stringWithFormat:@"text.fontSize: %.4f", command.fontSize]];
    [lines addObject:[NSString stringWithFormat:@"text.fontWeight: %d", command.fontWeight]];
    [lines addObject:[NSString stringWithFormat:@"text.lineHeight: %.4f", command.lineHeight]];
    [lines addObject:[NSString stringWithFormat:@"text.align: horizontal=%s vertical=%s", renderTextAlignHorizontalName(command.textAlignHorizontal), renderTextAlignVerticalName(command.textAlignVertical)]];

    const Figma::AssetDesc * asset = command.assetId.empty() == false ? [self assetForId:command.assetId] : nullptr;
    if(asset != nullptr)
    {
        [lines addObject:[NSString stringWithFormat:@"asset.id: %@", nsString(asset->id)]];
        [lines addObject:[NSString stringWithFormat:@"asset.path: %@", nsString(asset->path)]];
        [lines addObject:[NSString stringWithFormat:@"asset.mime: %@", nsString(asset->mime)]];
        [lines addObject:[NSString stringWithFormat:@"asset.size: %ux%u colorType=%u bytes=%zu", asset->width, asset->height, asset->colorType, asset->bytes.size()]];
    }
    else if(command.assetId.empty() == false)
    {
        [lines addObject:@"asset: missing"];
    }

    const std::size_t textLineSize = command.textLines.size();
    [lines addObject:[NSString stringWithFormat:@"textLines.count: %zu", textLineSize]];
    for(std::size_t lineIndex = 0; lineIndex != textLineSize; ++lineIndex)
    {
        const Figma::RenderTextLineDesc & line = command.textLines[lineIndex];
        [lines addObject:[NSString stringWithFormat:@"textLine[%zu]: x=%.3f y=%.3f width=%.3f lineHeight=%.3f lineAscent=%.3f text=%@",
                          lineIndex,
                          line.x,
                          line.y,
                          line.width,
                          line.lineHeight,
                          line.lineAscent,
                          escapedInspectorString(line.text)]];
    }

    const std::size_t vertexSize = command.vertices.size();
    [lines addObject:[NSString stringWithFormat:@"vertices.count: %zu", vertexSize]];
    for(std::size_t vertexIndex = 0; vertexIndex != vertexSize; ++vertexIndex)
    {
        const Figma::RenderVertex & vertex = command.vertices[vertexIndex];
        [lines addObject:[NSString stringWithFormat:@"vertex[%zu]: xy=(%.3f, %.3f) uv=(%.6f, %.6f) color=(%@)",
                          vertexIndex,
                          vertex.x,
                          vertex.y,
                          vertex.u,
                          vertex.v,
                          inspectorColorString(vertex.color)]];
    }

    const std::size_t indexSize = command.indices.size();
    [lines addObject:[NSString stringWithFormat:@"indices.count: %zu", indexSize]];
    for(std::size_t offset = 0; offset < indexSize; offset += 24)
    {
        const std::size_t end = std::min<std::size_t>(indexSize, offset + 24);
        std::ostringstream stream;
        stream << "indices[" << offset << ".." << (end - 1) << "]:";
        for(std::size_t index = offset; index != end; ++index)
        {
            stream << " " << command.indices[index];
        }

        const std::string value = stream.str();
        [lines addObject:[NSString stringWithUTF8String:value.c_str()]];
    }

    return lines;
}

//////////////////////////////////////////////////////////////////////////
- (NSString *)propertiesStringForCommandAtIndex:(std::size_t)_index
{
    [self syncCommandVisibility];

    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr || _index >= commands->size())
    {
        return @"";
    }

    NSArray<NSString *> * lines = [self detailLinesForCommand:&(*commands)[_index] index:_index];
    return [lines componentsJoinedByString:@"\n"];
}

//////////////////////////////////////////////////////////////////////////
- (void)copyCurrentCommandProperties:(id)sender
{
    (void)sender;

    if([self hasSelectedCommand] == NO)
    {
        return;
    }

    NSString * properties = [self propertiesStringForCommandAtIndex:static_cast<std::size_t>(m_selectedCommandIndex)];
    if(properties.length == 0)
    {
        return;
    }

    NSPasteboard * pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:properties forType:NSPasteboardTypeString];
    [self.window makeFirstResponder:self];
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)commandDetailsHeightAtIndex:(std::size_t)_index
{
    if([self isCommandExpandedAtIndex:_index] == NO)
    {
        return 0.0;
    }

    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr || _index >= commands->size())
    {
        return 0.0;
    }

    NSArray<NSString *> * detailLines = [self detailLinesForCommand:&(*commands)[_index] index:_index];
    return FigmaViewerInspectorDetailsVerticalPadding + static_cast<CGFloat>(detailLines.count) * FigmaViewerInspectorDetailsLineHeight;
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)commandRowHeightAtIndex:(std::size_t)_index
{
    return FigmaViewerInspectorRowHeight + [self commandDetailsHeightAtIndex:_index];
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)commandListContentHeight
{
    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr)
    {
        return 0.0;
    }

    CGFloat height = 0.0;
    const std::size_t commandSize = commands->size();
    for(std::size_t index = 0; index != commandSize; ++index)
    {
        height += [self commandRowHeightAtIndex:index];
    }

    return height;
}

//////////////////////////////////////////////////////////////////////////
- (void)clampCommandListScroll
{
    const NSRect listRect = [self inspectorListRectForInspectorRect:[self inspectorRect]];
    const CGFloat maxScroll = std::max<CGFloat>(0.0, [self commandListContentHeight] - listRect.size.height);
    m_commandListScroll = std::max<CGFloat>(0.0, std::min<CGFloat>(m_commandListScroll, maxScroll));
}

//////////////////////////////////////////////////////////////////////////
- (NSRect)commandRowRectAtIndex:(std::size_t)_index inListRect:(NSRect)_listRect
{
    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr || _index >= commands->size())
    {
        return NSZeroRect;
    }

    CGFloat y = _listRect.origin.y - m_commandListScroll;
    for(std::size_t index = 0; index != _index; ++index)
    {
        y += [self commandRowHeightAtIndex:index];
    }

    return NSMakeRect(_listRect.origin.x, y, _listRect.size.width, [self commandRowHeightAtIndex:_index]);
}

//////////////////////////////////////////////////////////////////////////
- (NSInteger)commandIndexAtInspectorPoint:(NSPoint)_point
{
    const NSRect inspector = [self inspectorRect];
    if(NSIsEmptyRect(inspector) == YES || NSPointInRect(_point, inspector) == NO)
    {
        return -1;
    }

    const NSRect listRect = [self inspectorListRectForInspectorRect:inspector];
    if(NSPointInRect(_point, listRect) == NO)
    {
        return -1;
    }

    const Figma::RenderCommandVector * commands = [self renderCommands];
    if(commands == nullptr)
    {
        return -1;
    }

    CGFloat y = listRect.origin.y - m_commandListScroll;
    const std::size_t commandSize = commands->size();
    for(std::size_t index = 0; index != commandSize; ++index)
    {
        const CGFloat height = [self commandRowHeightAtIndex:index];
        if(_point.y >= y && _point.y < y + height)
        {
            return static_cast<NSInteger>(index);
        }

        y += height;
    }

    return -1;
}

//////////////////////////////////////////////////////////////////////////
- (void)toggleCommandVisibilityAtIndex:(std::size_t)_index
{
    [self syncCommandVisibility];
    if(_index >= m_commandVisibility.size())
    {
        return;
    }

    m_commandVisibility[_index] = m_commandVisibility[_index] != 0 ? 0 : 1;
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)toggleCommandExpandedAtIndex:(std::size_t)_index
{
    [self syncCommandVisibility];
    if(_index >= m_commandExpanded.size())
    {
        return;
    }

    m_commandExpanded[_index] = m_commandExpanded[_index] != 0 ? 0 : 1;
    [self clampCommandListScroll];
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (NSRect)viewportArea
{
    const CGFloat inspectorWidth = [self inspectorWidth];
    return NSMakeRect(0.0, 0.0, std::max<CGFloat>(1.0, self.bounds.size.width - inspectorWidth), self.bounds.size.height);
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)baseViewportScale
{
    NSRect area = [self viewportArea];
    const CGFloat margin = 32.0;
    const CGFloat availableWidth = std::max<CGFloat>(1.0, area.size.width - margin * 2.0);
    const CGFloat availableHeight = std::max<CGFloat>(1.0, area.size.height - margin * 2.0);
    const CGFloat scaleX = availableWidth / std::max<CGFloat>(1.0, self.viewportWidth);
    const CGFloat scaleY = availableHeight / std::max<CGFloat>(1.0, self.viewportHeight);
    const CGFloat scale = std::min(scaleX, scaleY);
    return std::max<CGFloat>(0.1, std::min<CGFloat>(1.0, scale));
}

//////////////////////////////////////////////////////////////////////////
- (CGFloat)viewportScale
{
    return [self baseViewportScale] * std::max<CGFloat>(0.1, m_cameraZoom);
}

//////////////////////////////////////////////////////////////////////////
- (NSRect)screenRect
{
    NSRect area = [self viewportArea];
    const CGFloat scale = [self viewportScale];
    const CGFloat width = self.viewportWidth * scale;
    const CGFloat height = self.viewportHeight * scale;
    const CGFloat x = area.origin.x + (area.size.width - width) * 0.5 + m_cameraPan.width;
    const CGFloat y = area.origin.y + (area.size.height - height) * 0.5 + m_cameraPan.height;
    return NSMakeRect(std::floor(x), std::floor(y), std::ceil(width), std::ceil(height));
}

//////////////////////////////////////////////////////////////////////////
- (void)setCameraZoom:(CGFloat)zoom anchoredAtPoint:(NSPoint)point
{
    const CGFloat oldScale = [self viewportScale];
    const NSRect oldScreen = [self screenRect];
    const CGFloat viewportX = (point.x - oldScreen.origin.x) / std::max<CGFloat>(0.0001, oldScale);
    const CGFloat viewportY = (point.y - oldScreen.origin.y) / std::max<CGFloat>(0.0001, oldScale);

    m_cameraZoom = std::max<CGFloat>(0.25, std::min<CGFloat>(4.0, zoom));

    const NSRect area = [self viewportArea];
    const CGFloat newScale = [self viewportScale];
    const CGFloat newWidth = self.viewportWidth * newScale;
    const CGFloat newHeight = self.viewportHeight * newScale;
    const CGFloat baseX = area.origin.x + (area.size.width - newWidth) * 0.5;
    const CGFloat baseY = area.origin.y + (area.size.height - newHeight) * 0.5;
    m_cameraPan.width = point.x - viewportX * newScale - baseX;
    m_cameraPan.height = point.y - viewportY * newScale - baseY;
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)windowPoint:(NSPoint)point toViewportPoint:(Figma::Vec2f *)viewportPoint
{
    const NSRect screen = [self screenRect];
    if(NSPointInRect(point, screen) == NO)
    {
        return NO;
    }

    const CGFloat scale = [self viewportScale];
    viewportPoint->x = static_cast<float>((point.x - screen.origin.x) / scale);
    viewportPoint->y = static_cast<float>((point.y - screen.origin.y) / scale);

    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    const NSInteger commandIndex = [self commandIndexAtInspectorPoint:point];
    if(commandIndex >= 0)
    {
        m_selectedCommandIndex = commandIndex;
        [self updateCopyPropertiesButtonState];

        const NSRect listRect = [self inspectorListRectForInspectorRect:[self inspectorRect]];
        const NSRect rowRect = [self commandRowRectAtIndex:static_cast<std::size_t>(commandIndex) inListRect:listRect];
        const NSRect visibilityRect = NSMakeRect(rowRect.origin.x + 10.0, rowRect.origin.y + 3.0, 24.0, FigmaViewerInspectorRowHeight - 6.0);
        if(NSPointInRect(point, visibilityRect) == YES)
        {
            [self toggleCommandVisibilityAtIndex:static_cast<std::size_t>(commandIndex)];
        }
        else
        {
            [self toggleCommandExpandedAtIndex:static_cast<std::size_t>(commandIndex)];
        }

        [self.window makeFirstResponder:self];
        return;
    }

    if(m_spaceDown == YES)
    {
        m_cameraPanning = YES;
        m_lastPanPoint = point;
        return;
    }

    if(self.player == nullptr)
    {
        return;
    }

    Figma::Vec2f viewportPoint{};
    if([self windowPoint:point toViewportPoint:&viewportPoint] == NO)
    {
        return;
    }

    Figma::PointerEvent pointer;
    pointer.type = Figma::EPointerEventType::Down;
    pointer.x = viewportPoint.x;
    pointer.y = viewportPoint.y;
    pointer.button = pointerButtonFromEvent(event);
    pointer.modifiers = inputModifiersFromEvent(event);
    [self recordPlaybackPointerEvent:pointer];
    self.player->inputPointer(pointer);
    self.player->update(0.0f);
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)mouseDragged:(NSEvent *)event
{
    if(m_spaceDown == NO || m_cameraPanning == NO)
    {
        return;
    }

    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    m_cameraPan.width += point.x - m_lastPanPoint.x;
    m_cameraPan.height += point.y - m_lastPanPoint.y;
    m_lastPanPoint = point;
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)mouseUp:(NSEvent *)event
{
    if(m_cameraPanning == YES)
    {
        m_cameraPanning = NO;
        return;
    }

    if(self.player == nullptr)
    {
        return;
    }

    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    Figma::Vec2f viewportPoint{};
    if([self windowPoint:point toViewportPoint:&viewportPoint] == NO)
    {
        return;
    }

    Figma::PointerEvent pointer;
    pointer.type = Figma::EPointerEventType::Up;
    pointer.x = viewportPoint.x;
    pointer.y = viewportPoint.y;
    pointer.button = pointerButtonFromEvent(event);
    pointer.modifiers = inputModifiersFromEvent(event);
    [self recordPlaybackPointerEvent:pointer];
    self.player->inputPointer(pointer);
    self.player->update(0.0f);
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)scrollWheel:(NSEvent *)event
{
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    const NSRect inspector = [self inspectorRect];
    if(m_spaceDown == NO && NSIsEmptyRect(inspector) == NO && NSPointInRect(point, inspector) == YES)
    {
        const CGFloat delta = event.hasPreciseScrollingDeltas == YES ? event.scrollingDeltaY : event.deltaY * 10.0;
        m_commandListScroll -= delta;
        [self clampCommandListScroll];
        [self setNeedsDisplay:YES];
        return;
    }

    if(m_spaceDown == NO)
    {
        [super scrollWheel:event];
        return;
    }

    const CGFloat delta = event.hasPreciseScrollingDeltas == YES ? event.scrollingDeltaY : event.deltaY * 10.0;
    const CGFloat factor = std::exp(delta * 0.004);
    [self setCameraZoom:m_cameraZoom * factor anchoredAtPoint:point];
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (void)mouseMoved:(NSEvent *)event
{
    if(self.player == nullptr)
    {
        return;
    }

    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    Figma::Vec2f viewportPoint{};
    if([self windowPoint:point toViewportPoint:&viewportPoint] == NO)
    {
        return;
    }

    Figma::PointerEvent pointer;
    pointer.type = Figma::EPointerEventType::Move;
    pointer.x = viewportPoint.x;
    pointer.y = viewportPoint.y;
    pointer.modifiers = inputModifiersFromEvent(event);
    self.player->inputPointer(pointer);
    self.player->update(0.0f);
    [self setNeedsDisplay:YES];
}

//////////////////////////////////////////////////////////////////////////
- (const Figma::AssetDesc *)assetForId:(const Figma::FigmaString &)_assetId
{
    if(self.document == nullptr)
    {
        return nullptr;
    }

    return self.document->findAsset(std::string_view(_assetId.data(), _assetId.size()));
}

//////////////////////////////////////////////////////////////////////////
- (NSImage *)imageForAsset:(const Figma::AssetDesc *)asset
{
    if(asset == nullptr || asset->bytes.empty() == true)
    {
        return nil;
    }

    NSData * data = [NSData dataWithBytes:asset->bytes.data() length:asset->bytes.size()];
    return [[NSImage alloc] initWithData:data];
}

//////////////////////////////////////////////////////////////////////////
- (void)drawDiagnosticsInRect:(NSRect)rect
{
    if(self.player == nullptr)
    {
        return;
    }

    NSMutableParagraphStyle * paragraph = [[NSMutableParagraphStyle alloc] init];
    paragraph.lineBreakMode = NSLineBreakByTruncatingTail;
    NSDictionary * attrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.92 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11.0 weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: paragraph,
    };

    CGFloat row = rect.origin.y;
    const Figma::DiagnosticsInterface * diagnostics = self.player != nullptr ? self.player->getDiagnostics() : nullptr;
    if(diagnostics == nullptr)
    {
        return;
    }

    for(const Figma::Diagnostic & diagnostic : diagnostics->getItems())
    {
        NSString * line = [NSString stringWithFormat:@"%s: %@", diagnostic.code.c_str(), nsString(diagnostic.message)];
        [line drawInRect:NSMakeRect(rect.origin.x, row, rect.size.width, 16.0) withAttributes:attrs];
        row += 16.0;
        if(row > rect.origin.y + rect.size.height - 16.0)
        {
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)drawVisibilityIconInRect:(NSRect)_rect visible:(BOOL)_visible
{
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(context);
    [[NSColor colorWithCalibratedWhite:_visible == YES ? 0.82 : 0.34 alpha:1.0] setStroke];
    CGContextSetLineWidth(context, 1.4);
    CGContextStrokeEllipseInRect(context, CGRectMake(NSMidX(_rect) - 7.0, NSMidY(_rect) - 4.0, 14.0, 8.0));
    if(_visible == YES)
    {
        [[NSColor colorWithCalibratedWhite:0.82 alpha:1.0] setFill];
        CGContextFillEllipseInRect(context, CGRectMake(NSMidX(_rect) - 2.5, NSMidY(_rect) - 2.5, 5.0, 5.0));
    }
    else
    {
        CGContextMoveToPoint(context, NSMinX(_rect) + 5.0, NSMaxY(_rect) - 5.0);
        CGContextAddLineToPoint(context, NSMaxX(_rect) - 5.0, NSMinY(_rect) + 5.0);
        CGContextStrokePath(context);
    }
    CGContextRestoreGState(context);
}

//////////////////////////////////////////////////////////////////////////
- (void)drawInspectorInRect:(NSRect)_rect
{
    if(self.player == nullptr || NSIsEmptyRect(_rect) == YES)
    {
        return;
    }

    [self syncCommandVisibility];
    [self clampCommandListScroll];

    [[NSColor colorWithCalibratedRed:0.08 green:0.09 blue:0.10 alpha:0.98] setFill];
    NSRectFill(_rect);
    [[NSColor colorWithCalibratedWhite:0.22 alpha:1.0] setStroke];
    NSFrameRectWithWidth(_rect, 1.0);

    NSDictionary * titleAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.90 alpha:1.0],
        NSFontAttributeName: [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold],
    };
    NSMutableParagraphStyle * truncateParagraph = [[NSMutableParagraphStyle alloc] init];
    truncateParagraph.lineBreakMode = NSLineBreakByTruncatingTail;
    NSDictionary * rowAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.82 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:10.5 weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: truncateParagraph,
    };
    NSDictionary * hiddenRowAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.42 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:10.5 weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: truncateParagraph,
    };
    NSDictionary * markerAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.66 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:10.5 weight:NSFontWeightSemibold],
        NSParagraphStyleAttributeName: truncateParagraph,
    };
    NSDictionary * detailAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.68 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: truncateParagraph,
    };
    NSDictionary * hiddenDetailAttrs = @{
        NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.38 alpha:1.0],
        NSFontAttributeName: [NSFont monospacedSystemFontOfSize:9.0 weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: truncateParagraph,
    };

    const Figma::RenderCommandVector * commandsPtr = [self renderCommands];
    const std::size_t commandCount = commandsPtr != nullptr ? commandsPtr->size() : 0;
    NSString * title = [NSString stringWithFormat:@"Render List  %zu", commandCount];
    [title drawInRect:NSMakeRect(_rect.origin.x + 16.0, _rect.origin.y + 15.0, _rect.size.width - 132.0, 18.0) withAttributes:titleAttrs];

    const Figma::DiagnosticsInterface * diagnostics = self.player->getDiagnostics();
    const BOOL hasDiagnostics = diagnostics != nullptr && diagnostics->getItems().empty() == false;
    const CGFloat diagnosticsHeight = [self inspectorDiagnosticsHeight];
    const NSRect listRect = [self inspectorListRectForInspectorRect:_rect];

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(listRect);

    if(commandsPtr == nullptr)
    {
        [NSGraphicsContext restoreGraphicsState];
        return;
    }

    const Figma::RenderCommandVector & commands = *commandsPtr;
    CGFloat y = listRect.origin.y - m_commandListScroll;
    const std::size_t commandSize = commands.size();
    for(std::size_t index = 0; index != commandSize; ++index)
    {
        const CGFloat rowHeight = [self commandRowHeightAtIndex:index];
        if(y + rowHeight < listRect.origin.y)
        {
            y += rowHeight;
            continue;
        }

        if(y > NSMaxY(listRect))
        {
            break;
        }

        const Figma::RenderCommand & command = commands[index];
        const BOOL visible = [self isCommandVisibleAtIndex:index];
        const BOOL expanded = [self isCommandExpandedAtIndex:index];
        const BOOL selected = m_selectedCommandIndex >= 0 && static_cast<std::size_t>(m_selectedCommandIndex) == index ? YES : NO;
        const NSRect rowRect = NSMakeRect(listRect.origin.x, y, listRect.size.width, rowHeight);
        const NSRect baseRowRect = NSMakeRect(rowRect.origin.x, rowRect.origin.y, rowRect.size.width, FigmaViewerInspectorRowHeight);
        if(index % 2 == 0 || expanded == YES || selected == YES)
        {
            [[NSColor colorWithCalibratedWhite:selected == YES ? 0.18 : (expanded == YES ? 0.13 : 0.11) alpha:1.0] setFill];
            NSRectFill(rowRect);
        }

        [self drawVisibilityIconInRect:NSMakeRect(baseRowRect.origin.x + 10.0, baseRowRect.origin.y + 3.0, 24.0, baseRowRect.size.height - 6.0) visible:visible];

        NSString * marker = expanded == YES ? @"-" : @"+";
        [marker drawInRect:NSMakeRect(baseRowRect.origin.x + 40.0, baseRowRect.origin.y + 5.0, 14.0, 14.0) withAttributes:markerAttrs];

        const Figma::FigmaString & label = command.nodeId.empty() == false ? command.nodeId : command.id;
        NSString * row = [NSString stringWithFormat:@"%03zu  %s  %@", index, renderCommandTypeName(command.type), nsString(label)];
        [row drawInRect:NSMakeRect(baseRowRect.origin.x + 60.0, baseRowRect.origin.y + 5.0, baseRowRect.size.width - 70.0, 14.0)
         withAttributes:visible == YES ? rowAttrs : hiddenRowAttrs];

        if(expanded == YES)
        {
            NSArray<NSString *> * detailLines = [self detailLinesForCommand:&command index:index];
            CGFloat detailY = baseRowRect.origin.y + FigmaViewerInspectorRowHeight + 4.0;
            NSDictionary * attrs = visible == YES ? detailAttrs : hiddenDetailAttrs;
            for(NSString * line in detailLines)
            {
                [line drawInRect:NSMakeRect(rowRect.origin.x + 60.0, detailY, rowRect.size.width - 70.0, FigmaViewerInspectorDetailsLineHeight)
                  withAttributes:attrs];
                detailY += FigmaViewerInspectorDetailsLineHeight;
                if(detailY > NSMaxY(rowRect) || detailY > NSMaxY(listRect))
                {
                    break;
                }
            }
        }

        y += rowHeight;
    }

    [NSGraphicsContext restoreGraphicsState];

    if(hasDiagnostics == YES)
    {
        const NSRect diagnosticsRect = NSMakeRect(_rect.origin.x + 12.0, NSMaxY(_rect) - diagnosticsHeight + 10.0, _rect.size.width - 24.0, diagnosticsHeight - 18.0);
        [[NSColor colorWithCalibratedWhite:0.13 alpha:1.0] setFill];
        NSRectFill(NSInsetRect(diagnosticsRect, -6.0, -6.0));
        [self drawDiagnosticsInRect:diagnosticsRect];
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)drawScreenFrame
{
    const NSRect screen = [self screenRect];

    NSShadow * shadow = [[NSShadow alloc] init];
    shadow.shadowBlurRadius = 26.0;
    shadow.shadowOffset = NSMakeSize(0.0, -8.0);
    shadow.shadowColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.42];

    [NSGraphicsContext saveGraphicsState];
    [shadow set];
    [[NSColor colorWithCalibratedRed:0.0 green:0.0 blue:0.0 alpha:1.0] setFill];
    NSRectFill(screen);
    [NSGraphicsContext restoreGraphicsState];

    [[NSColor colorWithCalibratedRed:0.0 green:0.0 blue:0.0 alpha:1.0] setFill];
    NSRectFill(screen);

    [[NSColor colorWithCalibratedRed:0.18 green:0.20 blue:0.23 alpha:1.0] setStroke];
    NSFrameRectWithWidth(screen, 1.0);
}

//////////////////////////////////////////////////////////////////////////
- (void)drawWireframeCommand:(const Figma::RenderCommand &)_command
{
    if(_command.type == Figma::ERenderCommandType::ClipBegin || _command.type == Figma::ERenderCommandType::ClipEnd)
    {
        return;
    }

    if(_command.type == Figma::ERenderCommandType::DebugHotspot && self.showHotspots == NO)
    {
        return;
    }

    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, kCGBlendModeNormal);
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetShouldAntialias(context, true);
    CGContextSetLineWidth(context, std::max<CGFloat>(0.5, 1.0 / std::max<CGFloat>(0.0001, [self viewportScale])));
    [[NSColor colorWithSRGBRed:0.2 green:0.78 blue:1.0 alpha:m_wireframeMode == EViewerWireframeMode::Combined ? 0.72 : 0.95] setStroke];

    const std::size_t indexSize = _command.indices.size();
    const std::size_t vertexSize = _command.vertices.size();
    if(_command.vertices.empty() == false && indexSize >= 3)
    {
        CGContextBeginPath(context);
        for(std::size_t index = 0; index + 2 < indexSize; index += 3)
        {
            const std::uint16_t i0 = _command.indices[index + 0];
            const std::uint16_t i1 = _command.indices[index + 1];
            const std::uint16_t i2 = _command.indices[index + 2];
            if(i0 >= vertexSize || i1 >= vertexSize || i2 >= vertexSize)
            {
                continue;
            }

            const Figma::RenderVertex & v0 = _command.vertices[i0];
            const Figma::RenderVertex & v1 = _command.vertices[i1];
            const Figma::RenderVertex & v2 = _command.vertices[i2];
            CGContextMoveToPoint(context, static_cast<CGFloat>(v0.x), static_cast<CGFloat>(v0.y));
            CGContextAddLineToPoint(context, static_cast<CGFloat>(v1.x), static_cast<CGFloat>(v1.y));
            CGContextAddLineToPoint(context, static_cast<CGFloat>(v2.x), static_cast<CGFloat>(v2.y));
            CGContextClosePath(context);
        }
        CGContextStrokePath(context);
        CGContextRestoreGState(context);
        return;
    }

    const NSRect rect = NSMakeRect(_command.rect.x, _command.rect.y, _command.rect.w, _command.rect.h);
    CGContextBeginPath(context);
    addShapePath(context, _command.shape, rect, static_cast<CGFloat>(_command.cornerRadius));
    CGContextStrokePath(context);
    CGContextRestoreGState(context);
}

//////////////////////////////////////////////////////////////////////////
- (void)drawRenderCommand:(const Figma::RenderCommand &)_command
{
    const NSRect rect = NSMakeRect(_command.rect.x, _command.rect.y, _command.rect.w, _command.rect.h);
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, cgBlendModeForCommand(_command));
    if(m_wireframeMode != EViewerWireframeMode::Wireframe)
    {
        switch(_command.type)
        {
        case Figma::ERenderCommandType::Fill:
        {
            if(_command.vertices.empty() == false)
            {
                drawMeshCommand(_command);
                break;
            }

            [colorFromCommand(_command, 1.0) setFill];
            CGContextBeginPath(context);
            addShapePath(context, _command.shape, rect, static_cast<CGFloat>(_command.cornerRadius));
            CGContextFillPath(context);
            break;
        }
        case Figma::ERenderCommandType::Stroke:
        {
            if(_command.vertices.empty() == false)
            {
                drawMeshCommand(_command);
                break;
            }

            [colorFromCommand(_command, 1.0) setStroke];
            CGContextSetLineWidth(context, std::max<CGFloat>(0.0, _command.strokeWidth));
            CGContextBeginPath(context);
            addShapePath(context, _command.shape, rect, static_cast<CGFloat>(_command.cornerRadius));
            CGContextStrokePath(context);
            break;
        }
        case Figma::ERenderCommandType::Image:
        {
            const Figma::AssetDesc * asset = [self assetForId:_command.assetId];
            NSImage * image = [self imageForAsset:asset];
            if(image != nil)
            {
                drawImageCommand(image, asset, _command, rect);
            }
            break;
        }
        case Figma::ERenderCommandType::Text:
        {
            [self textRenderer]->drawText(_command, rect);
            break;
        }
        case Figma::ERenderCommandType::DebugHotspot:
        {
            if(self.showHotspots == YES)
            {
                [[NSColor colorWithCalibratedRed:1.0 green:0.62 blue:0.12 alpha:0.18] setFill];
                NSRectFillUsingOperation(rect, NSCompositingOperationSourceOver);
                [[NSColor colorWithCalibratedRed:1.0 green:0.62 blue:0.12 alpha:0.86] setStroke];
                CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
                CGContextSetLineWidth(context, 2.0 / [self viewportScale]);
                CGContextStrokeRect(context, cgRectFromNSRect(rect));
                NSDictionary * attrs = @{
                    NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.78 blue:0.28 alpha:1.0],
                    NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11.0 / [self viewportScale] weight:NSFontWeightBold],
                };
                [nsString(_command.text) drawInRect:NSInsetRect(rect, 8.0 / [self viewportScale], 8.0 / [self viewportScale]) withAttributes:attrs];
            }
            break;
        }
        case Figma::ERenderCommandType::Mesh:
            drawMeshCommand(_command);
            break;
        case Figma::ERenderCommandType::ClipBegin:
        case Figma::ERenderCommandType::ClipEnd:
            break;
        }
    }

    if(m_wireframeMode != EViewerWireframeMode::Normal)
    {
        [self drawWireframeCommand:_command];
    }
    CGContextRestoreGState(context);
}

//////////////////////////////////////////////////////////////////////////
- (void)drawRenderLayerCommands:(const Figma::RenderCommandVector &)_commands from:(std::size_t)_begin to:(std::size_t)_end opacity:(CGFloat)_opacity
{
    if(_begin >= _end || _opacity <= 0.0)
    {
        return;
    }

    bool hasVisibleCommand = false;
    for(std::size_t index = _begin; index != _end; ++index)
    {
        if([self isCommandVisibleAtIndex:index] == YES)
        {
            hasVisibleCommand = true;
            break;
        }
    }

    if(hasVisibleCommand == false)
    {
        return;
    }

    const NSSize layerSize = NSMakeSize(std::max<CGFloat>(1.0, self.viewportWidth), std::max<CGFloat>(1.0, self.viewportHeight));
    const CGFloat backingScale = std::max<CGFloat>(1.0, self.window != nil ? self.window.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor);
    const NSSize layerPixelSize = NSMakeSize(std::ceil(layerSize.width * backingScale), std::ceil(layerSize.height * backingScale));
    NSImage * layerImage = [[NSImage alloc] initWithSize:layerPixelSize];
    [layerImage lockFocusFlipped:YES];
    [[NSColor clearColor] setFill];
    NSRectFillUsingOperation(NSMakeRect(0.0, 0.0, layerPixelSize.width, layerPixelSize.height), NSCompositingOperationCopy);
    NSAffineTransform * backingTransform = [NSAffineTransform transform];
    [backingTransform scaleBy:backingScale];
    [backingTransform concat];
    for(std::size_t index = _begin; index != _end; ++index)
    {
        if([self isCommandVisibleAtIndex:index] == NO)
        {
            continue;
        }

        [self drawRenderCommand:_commands[index]];
    }
    [layerImage unlockFocus];
    layerImage.size = layerSize;

    [layerImage drawInRect:NSMakeRect(0.0, 0.0, layerSize.width, layerSize.height)
                  fromRect:NSMakeRect(0.0, 0.0, layerSize.width, layerSize.height)
                 operation:NSCompositingOperationSourceOver
                  fraction:std::clamp<CGFloat>(_opacity, 0.0, 1.0)
            respectFlipped:YES
                     hints:nil];
}

//////////////////////////////////////////////////////////////////////////
- (void)drawRenderList
{
    const Figma::RenderCommandVector * commandsPtr = [self renderCommands];
    if(commandsPtr == nullptr)
    {
        return;
    }

    const Figma::RenderCommandVector & commands = *commandsPtr;
    [self syncCommandVisibility];
    const std::size_t commandSize = commands.size();
    for(std::size_t index = 0; index != commandSize;)
    {
        const Figma::RenderCommand & command = commands[index];
        if(command.renderLayerId == 0)
        {
            if([self isCommandVisibleAtIndex:index] == YES)
            {
                [self drawRenderCommand:command];
            }
            ++index;
            continue;
        }

        const std::uint32_t layerId = command.renderLayerId;
        const CGFloat layerOpacity = static_cast<CGFloat>(command.renderLayerOpacity);
        const std::size_t begin = index;
        while(index != commandSize && commands[index].renderLayerId == layerId)
        {
            ++index;
        }

        [self drawRenderLayerCommands:commands from:begin to:index opacity:layerOpacity];
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)drawViewportToolingOverlay
{
    if(self.player == nullptr)
    {
        return;
    }

    if(m_wireframeMode == EViewerWireframeMode::Normal && self.showHotspots == NO)
    {
        return;
    }

    const Figma::RenderCommandVector * commandsPtr = [self renderCommands];
    if(commandsPtr == nullptr)
    {
        return;
    }

    const Figma::RenderCommandVector & commands = *commandsPtr;
    [self syncCommandVisibility];
    const std::size_t commandSize = commands.size();
    for(std::size_t index = 0; index != commandSize; ++index)
    {
        if([self isCommandVisibleAtIndex:index] == NO)
        {
            continue;
        }

        const Figma::RenderCommand & command = commands[index];
        if(m_wireframeMode == EViewerWireframeMode::Normal && command.type != Figma::ERenderCommandType::DebugHotspot)
        {
            continue;
        }

        [self drawWireframeCommand:command];
    }
}

//////////////////////////////////////////////////////////////////////////
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;

    [[NSColor colorWithCalibratedRed:0.06 green:0.07 blue:0.08 alpha:1.0] setFill];
    NSRectFill(self.bounds);

    if(self.player == nullptr)
    {
        if(m_metalLayer != nil)
        {
            m_metalLayer.hidden = YES;
        }
        return;
    }

    [self drawScreenFrame];
    if(m_wireframeMode == EViewerWireframeMode::Wireframe)
    {
        if(m_metalLayer != nil)
        {
            m_metalLayer.hidden = YES;
        }
    }
    else
    {
        if(m_metalLayer != nil)
        {
            m_metalLayer.hidden = NO;
            m_metalLayer.opacity = m_wireframeMode == EViewerWireframeMode::Combined ? 0.78f : 1.0f;
        }
        [self renderMetalViewport];
    }

    const NSRect screen = [self screenRect];
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(screen);
    NSAffineTransform * transform = [NSAffineTransform transform];
    [transform translateXBy:screen.origin.x yBy:screen.origin.y];
    [transform scaleBy:[self viewportScale]];
    [transform concat];
    if(m_wireframeMode == EViewerWireframeMode::Wireframe)
    {
        [self drawViewportToolingOverlay];
    }
    else if(m_wireframeMode == EViewerWireframeMode::Combined || self.showHotspots == YES)
    {
        [self drawViewportToolingOverlay];
    }
    [NSGraphicsContext restoreGraphicsState];

    const NSRect inspector = [self inspectorRect];
    if(NSIsEmptyRect(inspector) == NO)
    {
        [self drawInspectorInRect:inspector];
    }
    else
    {
        [self drawDiagnosticsInRect:NSMakeRect(16.0, self.bounds.size.height - 92.0, self.bounds.size.width - 32.0, 76.0)];
    }
}

@end
