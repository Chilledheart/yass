// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef YASS_MAC_VIEW_CONTROLLER
#define YASS_MAC_VIEW_CONTROLLER

#import <Cocoa/Cocoa.h>

@interface YassViewController : NSViewController
- (void)OnStart;
- (void)OnStop;
- (void)Started;
- (void)StartFailed;
- (void)Stopped;
@property(weak) IBOutlet NSButton* startButton;
@property(weak) IBOutlet NSButton* stopButton;
@property(weak) IBOutlet NSTextField* serverHost;
@property(weak) IBOutlet NSTextField* serverSNI;
@property(weak) IBOutlet NSTextField* serverPort;
@property(weak) IBOutlet NSTextField* username;
@property(weak) IBOutlet NSSecureTextField* password;
@property(weak) IBOutlet NSComboBox* cipherMethod;
@property(weak) IBOutlet NSTextField* localHost;
@property(weak) IBOutlet NSTextField* localPort;
@property(weak) IBOutlet NSTextField* dohURL;
@property(weak) IBOutlet NSTextField* dotHost;
@property(weak) IBOutlet NSTextField* limitRate;
@property(weak) IBOutlet NSTextField* timeout;
@property(weak) IBOutlet NSButton* autoStart;
@property(weak) IBOutlet NSButton* systemProxy;
@property(weak) IBOutlet NSButton* displayStatus;

+ (YassViewController* __weak)instance;
@end

#endif  // YASS_MAC_VIEW_CONTROLLER
