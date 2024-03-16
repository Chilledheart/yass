// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_OPTION_VIEW_CONTROLLER
#define YASS_OPTION_VIEW_CONTROLLER

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface OptionViewController : NSViewController
@property(weak) IBOutlet NSButton* tcpKeepAlive;
@property(weak) IBOutlet NSTextField* tcpKeepAliveCnt;
@property(weak) IBOutlet NSTextField* tcpKeepAliveIdleTimeout;
@property(weak) IBOutlet NSTextField* tcpKeepAliveInterval;

@end

NS_ASSUME_NONNULL_END

#endif  // YASS_OPTION_VIEW_CONTROLLER
