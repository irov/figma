#include "FigmaAppDelegate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

//////////////////////////////////////////////////////////////////////////
static NSString * fontListText(NSArray<NSString *> * _fonts, NSUInteger _maxCount)
{
    NSMutableArray<NSString *> * lines = [NSMutableArray array];
    const NSUInteger fontCount = _fonts.count;
    const NSUInteger lineCount = std::min<NSUInteger>(fontCount, _maxCount);

    for(NSUInteger index = 0; index != lineCount; ++index)
    {
        [lines addObject:[NSString stringWithFormat:@"- %@", _fonts[index]]];
    }

    if(fontCount > lineCount)
    {
        [lines addObject:[NSString stringWithFormat:@"- ... and %lu more", static_cast<unsigned long>(fontCount - lineCount)]];
    }

    return [lines componentsJoinedByString:@"\n"];
}

//////////////////////////////////////////////////////////////////////////
static NSString * missingFontsInformativeText(NSArray<NSString *> * _fonts, NSArray<NSString *> * _directories)
{
    NSString * fonts = fontListText(_fonts, 12);
    NSString * directories = [_directories componentsJoinedByString:@"\n"];

    return [NSString stringWithFormat:@"Missing fonts:\n%@\n\nChoose a folder containing .ttf, .otf, or .ttc files.\n\nCurrent search directories:\n%@",
                                      fonts,
                                      directories.length != 0 ? directories : @"<none>"];
}

@implementation FigmaAppDelegate

//////////////////////////////////////////////////////////////////////////
- (void)destroyFigmaObjects
{
    [self.timer invalidate];
    self.timer = nil;
    self.lastTickTime = nil;

    self.view.document = nullptr;
    self.view.player = nullptr;

    Figma::PlayerInterface * player = self.player;
    self.player = nullptr;
    destroyFigmaInterface(player);

    Figma::DocumentInterface * document = self.document;
    self.document = nullptr;
    destroyFigmaInterface(document);

    Figma::RuntimeInterface * runtime = self.runtime;
    self.runtime = nullptr;
    destroyFigmaInterface(runtime);
}

//////////////////////////////////////////////////////////////////////////
- (void)applicationWillTerminate:(NSNotification *)_notification
{
    (void)_notification;

    [self destroyFigmaObjects];
}

//////////////////////////////////////////////////////////////////////////
- (void)dealloc
{
    [self destroyFigmaObjects];
}

//////////////////////////////////////////////////////////////////////////
- (void)installMainMenu
{
    NSMenu * mainMenu = [[NSMenu alloc] initWithTitle:@""];

    NSMenuItem * appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:appItem];
    NSMenu * appMenu = [[NSMenu alloc] initWithTitle:@"Figma Viewer"];
    NSString * quitTitle = [NSString stringWithFormat:@"Quit %@", NSProcessInfo.processInfo.processName];
    [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;

    NSMenuItem * fileItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:fileItem];
    NSMenu * fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    NSMenuItem * openItem = [fileMenu addItemWithTitle:@"Open..." action:@selector(openDocument:) keyEquivalent:@"o"];
    openItem.target = self;
    fileItem.submenu = fileMenu;

    [NSApp setMainMenu:mainMenu];
}

//////////////////////////////////////////////////////////////////////////
- (void)showLoadErrorForPath:(NSString *)_path result:(Figma::EResult)_result
{
    NSAlert * alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = @"Unable to open Figma file";
    alert.informativeText = [NSString stringWithFormat:@"%@\n%s", _path ?: @"", resultToString(_result)];
    [alert addButtonWithTitle:@"OK"];
    if(self.window != nil)
    {
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    }
    else
    {
        [alert runModal];
    }
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)chooseFontDirectoryStartingAtPath:(NSString *)_path
{
    NSOpenPanel * panel = [NSOpenPanel openPanel];
    panel.title = @"Choose Fonts Folder";
    panel.prompt = @"Use Folder";
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.treatsFilePackagesAsDirectories = YES;
    if(_path.length != 0)
    {
        panel.directoryURL = [NSURL fileURLWithPath:_path.stringByDeletingLastPathComponent isDirectory:YES];
    }

    if([panel runModal] != NSModalResponseOK || panel.URL == nil)
    {
        return NO;
    }

    [self.view addFontSearchDirectory:panel.URL.path];
    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)promptForMissingFontsIfNeededNearPath:(NSString *)_path
{
    if(self.view == nil)
    {
        return;
    }

    for(;;)
    {
        NSArray<NSString *> * missingFonts = [self.view collectMissingFontDescriptions];
        if(missingFonts.count == 0)
        {
            return;
        }

        NSAlert * alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleWarning;
        alert.messageText = @"Missing Figma fonts";
        alert.informativeText = missingFontsInformativeText(missingFonts, [self.view fontSearchDirectories]);
        [alert addButtonWithTitle:@"Choose Fonts Folder"];
        [alert addButtonWithTitle:@"Continue Without Fonts"];

        if([alert runModal] != NSAlertFirstButtonReturn)
        {
            return;
        }

        if([self chooseFontDirectoryStartingAtPath:_path] == NO)
        {
            return;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)loadFigAtPath:(NSString *)_figPath sidecarPath:(NSString *)_sidecarPath showError:(BOOL)_showError
{
    if(self.runtime == nullptr || _figPath.length == 0)
    {
        return NO;
    }

    const std::string figPath = resolveFigPath(_figPath.fileSystemRepresentation);
    const char * sidecarPath = _sidecarPath.length > 0 ? _sidecarPath.fileSystemRepresentation : nullptr;

    Figma::DocumentInterface * document = nullptr;
    Figma::PlayerInterface * player = nullptr;
    Figma::PlayerDesc playerDesc;
    Figma::EResult result = loadViewerDocument(self.runtime, figPath, sidecarPath, &document, &player, &playerDesc);
    if(result != Figma::EResult::Ok)
    {
        std::fprintf(stderr, "load viewer document failed: %s\n", resultToString(result));
        if(_showError == YES)
        {
            [self showLoadErrorForPath:_figPath result:result];
        }
        return NO;
    }

    Figma::PlayerInterface * oldPlayer = self.player;
    Figma::DocumentInterface * oldDocument = self.document;

    self.document = document;
    self.player = player;
    self.window.title = [NSString stringWithFormat:@"Figma Viewer - %@", nsString(privateDocument(document)->getFileName())];
    [self.view configureWithDocument:document
                               player:player
                        viewportWidth:static_cast<CGFloat>(playerDesc.viewport.width)
                       viewportHeight:static_cast<CGFloat>(playerDesc.viewport.height)];

    destroyFigmaInterface(oldPlayer);
    destroyFigmaInterface(oldDocument);

    [self.window makeFirstResponder:self.view];
    self.lastTickTime = [NSDate date];
    [self refreshTimerMode];
    [self promptForMissingFontsIfNeededNearPath:_figPath];
    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)openDocument:(id)_sender
{
    (void)_sender;

    NSOpenPanel * panel = [NSOpenPanel openPanel];
    panel.title = @"Open Figma File";
    panel.prompt = @"Open";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.treatsFilePackagesAsDirectories = NO;
    if(self.document != nullptr)
    {
        NSString * path = nsString(privateDocument(self.document)->getPath());
        panel.directoryURL = [NSURL fileURLWithPath:path.stringByDeletingLastPathComponent isDirectory:YES];
    }

    if([panel runModal] != NSModalResponseOK)
    {
        return;
    }

    NSURL * url = panel.URL;
    if(url == nil)
    {
        return;
    }

    [self loadFigAtPath:url.path sidecarPath:nil showError:YES];
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)shouldRunRenderLoop
{
    if(self.window == nil || self.view == nil)
    {
        return NO;
    }

    if(NSApp.hidden == YES || self.window.visible == NO || self.window.miniaturized == YES)
    {
        return NO;
    }

    return (self.window.occlusionState & NSWindowOcclusionStateVisible) != 0;
}

//////////////////////////////////////////////////////////////////////////
- (void)scheduleTimerWithInterval:(NSTimeInterval)interval
{
    if(self.timer != nil && std::fabs(self.timerInterval - interval) <= 0.0001)
    {
        return;
    }

    [self.timer invalidate];
    self.timerInterval = interval;
    self.timer = [NSTimer scheduledTimerWithTimeInterval:interval target:self selector:@selector(tick:) userInfo:nil repeats:YES];
    self.timer.tolerance = std::min<NSTimeInterval>(interval * 0.25, 0.01);
}

//////////////////////////////////////////////////////////////////////////
- (void)refreshTimerMode
{
    const BOOL runRenderLoop = [self shouldRunRenderLoop];
    const BOOL paused = self.view != nil && self.view.playbackPaused == YES;
    const BOOL hasPlayer = self.player != nullptr;
    [self scheduleTimerWithInterval:(runRenderLoop == YES && paused == NO && hasPlayer == YES ? FigmaViewerActiveFrameInterval : FigmaViewerIdleTimerInterval)];
    self.lastTickTime = [NSDate date];
}

//////////////////////////////////////////////////////////////////////////
- (NSDate *)sleepUntilNextActiveFrameFromDate:(NSDate *)_now
{
    if(self.lastTickTime == nil)
    {
        return _now;
    }

    const NSTimeInterval elapsed = std::max<NSTimeInterval>(0.0, [_now timeIntervalSinceDate:self.lastTickTime]);
    const NSTimeInterval remaining = FigmaViewerActiveFrameInterval - elapsed;
    if(remaining <= 0.0005)
    {
        return _now;
    }

    [NSThread sleepForTimeInterval:remaining];
    return [NSDate date];
}

//////////////////////////////////////////////////////////////////////////
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;
    return YES;
}

//////////////////////////////////////////////////////////////////////////
- (void)windowWillClose:(NSNotification *)notification
{
    (void)notification;
    [self.timer invalidate];
    self.timer = nil;
    [NSApp terminate:nil];
}

//////////////////////////////////////////////////////////////////////////
- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)applicationDidHide:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)applicationDidUnhide:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)windowDidMiniaturize:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)windowDidChangeOcclusionState:(NSNotification *)notification
{
    (void)notification;
    [self refreshTimerMode];
}

//////////////////////////////////////////////////////////////////////////
- (void)tick:(NSTimer *)timer
{
    (void)timer;

    if(self.player == nullptr || self.view == nil)
    {
        [self scheduleTimerWithInterval:FigmaViewerIdleTimerInterval];
        self.lastTickTime = [NSDate date];
        return;
    }

    NSDate * now = [NSDate date];
    if([self shouldRunRenderLoop] == NO)
    {
        [self scheduleTimerWithInterval:FigmaViewerIdleTimerInterval];
        self.lastTickTime = now;
        return;
    }

    if(self.view.playbackPaused == YES)
    {
        [self scheduleTimerWithInterval:FigmaViewerIdleTimerInterval];
        self.lastTickTime = now;
        return;
    }

    [self scheduleTimerWithInterval:FigmaViewerActiveFrameInterval];
    now = [self sleepUntilNextActiveFrameFromDate:now];

    NSTimeInterval dt = 0.0;
    if(self.lastTickTime != nil)
    {
        dt = [now timeIntervalSinceDate:self.lastTickTime];
    }
    self.lastTickTime = now;

    const CGFloat playbackSpeed = std::max<CGFloat>(0.0, self.view.playbackSpeed);
    [self.view advancePlaybackBy:std::max<NSTimeInterval>(0.0, std::min<NSTimeInterval>(0.1, dt)) * playbackSpeed];
}

@end
