// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef YASS_IOS_APP_DELEGATE
#define YASS_IOS_APP_DELEGATE

#import <UIKit/UIKit.h>

enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };

@interface YassAppDelegate : UIResponder <UIApplicationDelegate>
- (enum YASSState) getState;
- (NSString*) getStatus;
- (void) OnStart:(BOOL) quiet;
- (void) OnStop:(BOOL) quiet;

@end

#endif // YASS_IOS_APP_DELEGATE
