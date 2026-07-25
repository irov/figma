#pragma once

#include "FigmaViewerShared.h"

@interface FigmaViewerView : NSView
@property(nonatomic) figma_document_t * document;
@property(nonatomic) figma_player_t * player;
@property(nonatomic) BOOL showHotspots;
@property(nonatomic) BOOL playbackPaused;
@property(nonatomic) CGFloat playbackSpeed;
@property(nonatomic) CGFloat viewportWidth;
@property(nonatomic) CGFloat viewportHeight;
- (void)configureWithDocument:(figma_document_t *)_newDocument player:(figma_player_t *)_newPlayer viewportWidth:(CGFloat)_newViewportWidth viewportHeight:(CGFloat)_newViewportHeight;
- (void)advancePlaybackBy:(NSTimeInterval)_dt;
- (NSArray<NSString *> *)collectMissingFontDescriptions;
- (NSArray<NSString *> *)fontSearchDirectories;
- (void)addFontSearchDirectory:(NSString *)_directory;
@end
