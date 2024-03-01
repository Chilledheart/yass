// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef YASS_MAC_WINDOW_CONTROLLER
#define YASS_MAC_WINDOW_CONTROLLER

#import <Cocoa/Cocoa.h>

@interface YassWindowController : NSWindowController <NSWindowDelegate>
- (void)OnStart;
- (void)OnStop;
- (void)Started;
- (void)StartFailed;
- (void)Stopped;
@end

#endif  // YASS_MAC_WINDOW_CONTROLLER
