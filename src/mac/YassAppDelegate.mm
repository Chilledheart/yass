// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#import "mac/YassAppDelegate.h"

#include "cli/cli_worker.hpp"

#include <pthread.h>
#include <stdexcept>
#include <string>

#include <absl/flags/flag.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "mac/OptionViewController.h"
#include "mac/YassViewController.h"
#include "mac/utils.h"

@interface YassAppDelegate ()
- (void)SaveConfig;
- (void)OnStarted;
- (void)OnStartFailed:(std::string)error_msg;
- (void)OnStopped;
@end

@implementation YassAppDelegate {
  enum YASSState state_;
  std::string error_msg_;
  Worker worker_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  state_ = STOPPED;
  if (CheckLoginItemStatus(nullptr)) {
    [viewController OnStart];
  }

  // https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  LOG(WARNING) << "Application exiting";
  [self OnStop:TRUE];
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}

- (IBAction)OnOptionMenuClicked:(id)sender {
  NSStoryboard* storyboard = [NSStoryboard storyboardWithName:@"Main"
                                                       bundle:nil];
  OptionViewController* optionViewController =
      (OptionViewController*)[storyboard
          instantiateControllerWithIdentifier:@"OptionViewController"];
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  [viewController presentViewControllerAsModalWindow:optionViewController];
}

- (enum YASSState)getState {
  return state_;
}

- (NSString*)getStatus {
  std::ostringstream ss;
  if (state_ == STARTED) {
    NSString *prefixMessage = NSLocalizedString(@"CONNECTED_WITH_CONNS", @"Connected with conns: ");
    ss << SysNSStringToUTF8(prefixMessage) << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    NSString *prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else {
    NSString *prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << SysNSStringToUTF8(prefixMessage) << worker_.GetRemoteDomain();
  }

  return SysUTF8ToNSString(ss.str());
}

- (void)OnStart:(BOOL)quiet {
  state_ = STARTING;
  [self SaveConfig];

  std::function<void(asio::error_code)> callback;
  if (!quiet) {
    callback = [=](asio::error_code ec) {
      bool successed = false;
      std::string msg;

      if (ec) {
        msg = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      dispatch_async(dispatch_get_main_queue(), ^{
        if (successed) {
          [self OnStarted];
        } else {
          [self OnStartFailed:msg];
        }
      });
    };
  }
  worker_.Start(callback);
}

- (void)OnStop:(BOOL)quiet {
  state_ = STOPPING;

  std::function<void()> callback;
  if (!quiet) {
    callback = [=]() {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self OnStopped];
      });
    };
  }
  worker_.Stop(callback);
}

- (void)OnStarted {
  state_ = STARTED;
  config::SaveConfig();

  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  [viewController Started];
}

- (void)OnStartFailed:(std::string)error_msg {
  state_ = START_FAILED;

  error_msg_ = error_msg;
  LOG(ERROR) << "worker failed due to: " << error_msg_;
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  [viewController StartFailed];
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @(error_msg.c_str());
  alert.icon = [NSImage imageNamed:NSImageNameCaution];
  alert.alertStyle = NSAlertStyleWarning;
  [alert runModal];
}

- (void)OnStopped {
  state_ = STOPPED;
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  [viewController Stopped];
}

- (void)SaveConfig {
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  auto server_host = SysNSStringToUTF8(viewController.serverHost.stringValue);
  auto server_port = viewController.serverPort.intValue;
  auto username = SysNSStringToUTF8(viewController.username.stringValue);
  auto password = SysNSStringToUTF8(viewController.password.stringValue);
  auto method_string =
      SysNSStringToUTF8(viewController.cipherMethod.stringValue);
  auto method = to_cipher_method(method_string);
  auto local_host = SysNSStringToUTF8(viewController.localHost.stringValue);
  auto local_port = viewController.localPort.intValue;
  auto connect_timeout = viewController.timeout.intValue;

  if (method == CRYPTO_INVALID) {
    LOG(WARNING) << "invalid options";
    return;
  }

  absl::SetFlag(&FLAGS_server_host, server_host);
  absl::SetFlag(&FLAGS_server_port, server_port);
  absl::SetFlag(&FLAGS_username, username);
  absl::SetFlag(&FLAGS_password, password);
  absl::SetFlag(&FLAGS_method, method);
  absl::SetFlag(&FLAGS_local_host, local_host);
  absl::SetFlag(&FLAGS_local_port, local_port);
  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout);
}

@end
