// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#import "mac/YassViewController.h"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "mac/YassWindowController.h"
#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
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
  for (uint32_t i = 0; i < sizeof(methodStrings) / sizeof(methodStrings[0]);
       ++i) {
    [self.cipherMethod addItemWithObjectValue:methodStrings[i]];
  }
  self.cipherMethod.numberOfVisibleItems =
  sizeof(methodStrings) / sizeof(methodStrings[0]);

  [self.autoStart
   setState:(CheckLoginItemStatus(nullptr) ? NSControlStateValueOn
             : NSControlStateValueOff)];
  [self.systemProxy
   setState:(GetSystemProxy() ? NSControlStateValueOn : NSControlStateValueOff)];
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
  dispatch_async(
    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      SetSystemProxy(enable);
      dispatch_async(dispatch_get_main_queue(), ^{
        [self.systemProxy setEnabled:TRUE];
      });
  });
}

- (void)OnStart {
  [self.startButton setEnabled:FALSE];

  YassWindowController* windowController = (YassWindowController*)self.view.window.windowController;
  [windowController OnStart];
}

- (void)OnStop {
  [self.stopButton setEnabled:FALSE];
  YassWindowController* windowController = (YassWindowController*)self.view.window.windowController;
  [windowController OnStop];
}

- (void)Started {
  [self.serverHost setEditable:FALSE];
  [self.serverSNI setEditable:FALSE];
  [self.serverPort setEditable:FALSE];
  [self.username setEditable:FALSE];
  [self.password setEditable:FALSE];
  [self.cipherMethod setEditable:FALSE];
  [self.localHost setEditable:FALSE];
  [self.localPort setEditable:FALSE];
  [self.timeout setEditable:FALSE];
  [self.stopButton setEnabled:TRUE];
}

- (void)StartFailed {
  [self.serverHost setEditable:TRUE];
  [self.serverSNI setEditable:TRUE];
  [self.serverPort setEditable:TRUE];
  [self.username setEditable:TRUE];
  [self.password setEditable:TRUE];
  [self.cipherMethod setEditable:TRUE];
  [self.localHost setEditable:TRUE];
  [self.localPort setEditable:TRUE];
  [self.timeout setEditable:TRUE];
  [self.startButton setEnabled:TRUE];
}

- (void)Stopped {
  [self.serverHost setEditable:TRUE];
  [self.serverSNI setEditable:TRUE];
  [self.serverPort setEditable:TRUE];
  [self.username setEditable:TRUE];
  [self.password setEditable:TRUE];
  [self.cipherMethod setEditable:TRUE];
  [self.localHost setEditable:TRUE];
  [self.localPort setEditable:TRUE];
  [self.timeout setEditable:TRUE];
  [self.startButton setEnabled:TRUE];
}

- (void)LoadChanges {
  self.serverHost.stringValue =
      gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_server_host));
  self.serverSNI.stringValue =
      gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_server_sni));
  self.serverPort.intValue = absl::GetFlag(FLAGS_server_port);
  self.username.stringValue = gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_username));
  self.password.stringValue = gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_password));
  auto cipherMethod = absl::GetFlag(FLAGS_method).method;
  self.cipherMethod.stringValue =
      gurl_base::SysUTF8ToNSString(to_cipher_method_str(cipherMethod));
  self.localHost.stringValue =
      gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_local_host));
  self.localPort.intValue = absl::GetFlag(FLAGS_local_port);
  self.timeout.intValue = absl::GetFlag(FLAGS_connect_timeout);
}

@end
