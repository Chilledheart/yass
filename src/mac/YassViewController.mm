// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#import "mac/YassViewController.h"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "mac/YassWindowController.h"
#include "mac/utils.h"

@interface YassViewController ()
@end

@implementation YassViewController {
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.cipherMethod removeAllItems];
  NSString* methodStrings[] = {
#define XX(num, name, string) @string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  for (NSString* methodString : methodStrings) {
    [self.cipherMethod addItemWithObjectValue:methodString];
  }
  self.cipherMethod.numberOfVisibleItems = sizeof(methodStrings) / sizeof(methodStrings[0]);

  [self.autoStart setState:(CheckLoginItemStatus(nullptr) ? NSControlStateValueOn : NSControlStateValueOff)];
  [self.systemProxy setState:(GetSystemProxy() ? NSControlStateValueOn : NSControlStateValueOff)];
  [self LoadChanges];
  [self.startButton setEnabled:TRUE];
  [self.stopButton setEnabled:FALSE];
}

- (void)viewWillAppear {
  [self.view.window center];
}

- (IBAction)OnStartButtonClicked:(id)sender {
  [self OnStart];
}

- (IBAction)OnStopButtonClicked:(id)sender {
  [self OnStop];
}

- (IBAction)OnAutoStartChecked:(id)sender {
  bool enable = self.autoStart.state == NSControlStateValueOn;
  if (enable && !CheckLoginItemStatus(nullptr)) {
    AddToLoginItems(true);
  } else {
    RemoveFromLoginItems();
  }
}

- (IBAction)OnSystemProxyChecked:(id)sender {
  bool enable = self.systemProxy.state == NSControlStateValueOn;
  [self.systemProxy setEnabled:FALSE];
  // this operation might be slow, moved to background
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    SetSystemProxy(enable);
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.systemProxy setEnabled:TRUE];
    });
  });
}

- (void)OnStart {
  [self.startButton setEnabled:FALSE];
  [self.stopButton setEnabled:FALSE];

  [self.serverHost setEnabled:FALSE];
  [self.serverSNI setEnabled:FALSE];
  [self.serverPort setEnabled:FALSE];
  [self.username setEnabled:FALSE];
  [self.password setEnabled:FALSE];
  [self.cipherMethod setEnabled:FALSE];
  [self.localHost setEnabled:FALSE];
  [self.localPort setEnabled:FALSE];
  [self.dohURL setEnabled:FALSE];
  [self.dotHost setEnabled:FALSE];
  [self.timeout setEnabled:FALSE];

  YassWindowController* windowController = (YassWindowController*)self.view.window.windowController;
  [windowController OnStart];
}

- (void)OnStop {
  [self.startButton setEnabled:FALSE];
  [self.stopButton setEnabled:FALSE];

  YassWindowController* windowController = (YassWindowController*)self.view.window.windowController;
  [windowController OnStop];
}

- (void)Started {
  [self.startButton setEnabled:FALSE];
  [self.stopButton setEnabled:TRUE];
}

- (void)StartFailed {
  [self.startButton setEnabled:TRUE];
  [self.stopButton setEnabled:FALSE];

  [self.serverHost setEnabled:TRUE];
  [self.serverSNI setEnabled:TRUE];
  [self.serverPort setEnabled:TRUE];
  [self.username setEnabled:TRUE];
  [self.password setEnabled:TRUE];
  [self.cipherMethod setEnabled:TRUE];
  [self.localHost setEnabled:TRUE];
  [self.localPort setEnabled:TRUE];
  [self.dohURL setEnabled:TRUE];
  [self.dotHost setEnabled:TRUE];
  [self.timeout setEnabled:TRUE];
}

- (void)Stopped {
  [self.startButton setEnabled:TRUE];
  [self.stopButton setEnabled:FALSE];

  [self.serverHost setEnabled:TRUE];
  [self.serverSNI setEnabled:TRUE];
  [self.serverPort setEnabled:TRUE];
  [self.username setEnabled:TRUE];
  [self.password setEnabled:TRUE];
  [self.cipherMethod setEnabled:TRUE];
  [self.localHost setEnabled:TRUE];
  [self.localPort setEnabled:TRUE];
  [self.dohURL setEnabled:TRUE];
  [self.dotHost setEnabled:TRUE];
  [self.timeout setEnabled:TRUE];
}

- (void)LoadChanges {
  self.serverHost.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_server_host));
  self.serverSNI.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_server_sni));
  self.serverPort.intValue = absl::GetFlag(FLAGS_server_port);
  self.username.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_username));
  self.password.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_password));
  auto cipherMethod = absl::GetFlag(FLAGS_method).method;
  self.cipherMethod.stringValue = SysUTF8ToNSString(to_cipher_method_str(cipherMethod));
  self.localHost.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_local_host));
  self.localPort.intValue = absl::GetFlag(FLAGS_local_port);
  self.dohURL.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_doh_url));
  self.dotHost.stringValue = SysUTF8ToNSString(absl::GetFlag(FLAGS_dot_host));
  self.timeout.intValue = absl::GetFlag(FLAGS_connect_timeout);
}

@end
