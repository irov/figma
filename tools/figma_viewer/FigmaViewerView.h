#pragma once

#include "FigmaViewerShared.h"

@interface FigmaViewerView : NSView
@property(nonatomic) Figma::DocumentInterface * document;
@property(nonatomic) Figma::PlayerInterface * player;
@property(nonatomic) BOOL showHotspots;
@property(nonatomic) BOOL playbackPaused;
@property(nonatomic) CGFloat playbackSpeed;
@property(nonatomic) CGFloat viewportWidth;
@property(nonatomic) CGFloat viewportHeight;
- (void)configureWithDocument:(Figma::DocumentInterface *)_newDocument player:(Figma::PlayerInterface *)_newPlayer viewportWidth:(CGFloat)_newViewportWidth viewportHeight:(CGFloat)_newViewportHeight;
- (void)advancePlaybackBy:(NSTimeInterval)_dt;
- (NSArray<NSString *> *)collectMissingFontDescriptions;
- (NSArray<NSString *> *)fontSearchDirectories;
- (void)addFontSearchDirectory:(NSString *)_directory;
@end
