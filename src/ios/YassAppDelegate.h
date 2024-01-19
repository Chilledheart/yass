// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef YASS_IOS_APP_DELEGATE
#define YASS_IOS_APP_DELEGATE

#import <UIKit/UIKit.h>

enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };

@interface YassAppDelegate : UIResponder <UIApplicationDelegate>
- (void) reloadState;
- (enum YASSState) getState;
- (NSString*) getStatus;
- (void) OnStart:(BOOL) quiet;
- (void) OnStop:(BOOL) quiet;
@property (assign, nonatomic) uint64_t total_rx_bytes;
@property (assign, nonatomic) uint64_t total_tx_bytes;
@end

#endif // YASS_IOS_APP_DELEGATE
