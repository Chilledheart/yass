// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_OPTION_VIEW_CONTROLLER
#define YASS_OPTION_VIEW_CONTROLLER

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface OptionViewController : NSViewController
@property (weak) IBOutlet NSTextField *connectTimeout;
@property (weak) IBOutlet NSTextField *tcpUserTimeout;
@property (weak) IBOutlet NSTextField *tcpSoLingerTimeout;
@property (weak) IBOutlet NSTextField *tcpSendBuffer;
@property (weak) IBOutlet NSTextField *tcpRecevieBuffer;

@end

NS_ASSUME_NONNULL_END

#endif // YASS_OPTION_VIEW_CONTROLLER
