// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_MAC_APP_DELEGATE
#define YASS_MAC_APP_DELEGATE

#import <Cocoa/Cocoa.h>

enum YASSState { STOPPED, STARTED, STARTING, START_FAILED, STOPPING };

@interface YassAppDelegate : NSObject <NSApplicationDelegate>
- (enum YASSState)getState;
- (NSString*)getStatus;
- (void)OnStart;
- (void)OnStop:(BOOL)quiet;

@end

#endif  // YASS_MAC_APP_DELEGATE
