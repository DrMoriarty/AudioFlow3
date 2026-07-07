#import <Cocoa/Cocoa.h>
#include <QApplication>
#include <QMainWindow>

extern void cleanup();

static QMainWindow *g_mainWindow = nullptr;

@interface DockReopenDelegate : NSObject <NSApplicationDelegate>
@end

@implementation DockReopenDelegate

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if (g_mainWindow && !flag) {
        g_mainWindow->show();
        g_mainWindow->raise();
        g_mainWindow->activateWindow();
    }
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    cleanup();
    return NSTerminateNow;
}

@end

extern "C" void setupDockReopen(QMainWindow *window)
{
    g_mainWindow = window;
    static DockReopenDelegate *delegate = [[DockReopenDelegate alloc] init];
    [NSApp setDelegate:delegate];
}
