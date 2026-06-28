#import <AVFoundation/AVFoundation.h>
#include <dispatch/dispatch.h>
#include <iostream>

extern "C" {
void requestMicrophonePermission() {
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block bool granted = false;
    
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL g) {
        granted = g;
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    
    std::cerr << "[main] Microphone permission: " << (granted ? "granted" : "denied") << std::endl;
    dispatch_release(semaphore);
}
}
