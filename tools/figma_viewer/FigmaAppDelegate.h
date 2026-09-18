#pragma once

#include "FigmaViewerView.h"

@interface FigmaAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, strong) NSWindow * window;
@property(nonatomic, strong) FigmaViewerView * view;
@property(nonatomic) figma_runtime_t * runtime;
@property(nonatomic) gp_graphics_t * graphics;
@property(nonatomic) figma_document_t * document;
@property(nonatomic) figma_player_t * player;
@property(nonatomic, strong) NSTimer * timer;
@property(nonatomic, strong) NSDate * lastTickTime;
@property(nonatomic) NSTimeInterval timerInterval;
- (void)installMainMenu;
- (BOOL)loadFigAtPath:(NSString *)_figPath sidecarPath:(NSString *)_sidecarPath showError:(BOOL)_showError;
- (void)openDocument:(id)_sender;
- (void)refreshTimerMode;
- (void)destroyFigmaObjects;
@end
