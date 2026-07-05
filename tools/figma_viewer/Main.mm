#include "FigmaAppDelegate.h"
#include "FigmaViewerShared.h"

#include <algorithm>
#include <cstdio>
#include <string>

//////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
    @autoreleasepool
    {
        Figma::RuntimeInterface * runtimePtr = nullptr;
        Figma::EResult result = Figma::createRuntime({}, &runtimePtr);
        if(result != Figma::EResult::Ok)
        {
            std::fprintf(stderr, "createRuntime failed: %s\n", resultToString(result));
            return 1;
        }
        FigmaInterfaceOwner<Figma::RuntimeInterface> runtime(runtimePtr);

        Figma::DocumentInterface * documentPtr = nullptr;
        Figma::PlayerInterface * playerPtr = nullptr;
        Figma::PlayerDesc playerDesc = makePlayerDesc(nullptr);
        if(argc > 1)
        {
            const std::string figPath = resolveFigPath(argv[1]);
            const char * sidecarPath = argc > 2 ? argv[2] : nullptr;
            result = loadViewerDocument(runtime.get(), figPath, sidecarPath, &documentPtr, &playerPtr, &playerDesc);
            if(result != Figma::EResult::Ok)
            {
                std::fprintf(stderr, "load viewer document failed: %s\n", resultToString(result));
                return 1;
            }
        }
        FigmaInterfaceOwner<Figma::DocumentInterface> document(documentPtr);
        FigmaInterfaceOwner<Figma::PlayerInterface> player(playerPtr);

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        FigmaAppDelegate * delegate = [[FigmaAppDelegate alloc] init];
        delegate.runtime = runtime.release();
        delegate.document = document.release();
        delegate.player = player.release();
        [NSApp setDelegate:delegate];
        [delegate installMainMenu];

        const CGFloat windowWidth = std::max<CGFloat>(720.0, static_cast<CGFloat>(playerDesc.viewport.width) + 320.0);
        const CGFloat windowHeight = std::max<CGFloat>(820.0, static_cast<CGFloat>(playerDesc.viewport.height) + 112.0);
        NSRect windowRect = NSMakeRect(0, 0, windowWidth, windowHeight);
        NSWindow * window = [[NSWindow alloc] initWithContentRect:windowRect
                                                        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
        window.title = delegate.document != nullptr ? [NSString stringWithFormat:@"Figma Viewer - %@", nsString(privateDocument(delegate.document)->getFileName())] : @"Figma Viewer";

        FigmaViewerView * view = [[FigmaViewerView alloc] initWithFrame:windowRect];
        view.showHotspots = NO;
        [view configureWithDocument:delegate.document
                              player:delegate.player
                       viewportWidth:static_cast<CGFloat>(playerDesc.viewport.width)
                      viewportHeight:static_cast<CGFloat>(playerDesc.viewport.height)];
        window.contentView = view;
        window.delegate = delegate;
        delegate.window = window;
        delegate.view = view;

        [window center];
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];
        [NSApp activateIgnoringOtherApps:YES];
        delegate.lastTickTime = [NSDate date];
        [delegate refreshTimerMode];
        [NSApp run];
    }

    return 0;
}
