// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#import "mac/YassAppDelegate.h"

#include "cli/cli_worker.hpp"

#include <pthread.h>
#include <stdexcept>
#include <string>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "mac/OptionViewController.h"
#include "mac/YassViewController.h"
#include "mac/utils.h"
#include "version.h"
#include "feature.h"

@interface YassAppDelegate ()
- (BOOL)SaveConfig;
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
  [NSApp activateIgnoringOtherApps:true];
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  LOG(WARNING) << "Application exiting";
  [self OnStop:TRUE];
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}

- (IBAction)OnAboutMenuClicked:(id)sender {
  NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary<NSAboutPanelOptionKey, id> *dict = @{
    NSAboutPanelOptionApplicationName : @(YASS_APP_PRODUCT_NAME),
    NSAboutPanelOptionApplicationVersion : @(YASS_APP_PRODUCT_VERSION),
    NSAboutPanelOptionVersion : @(YASS_APP_LAST_CHANGE),
    NSAboutPanelOptionCredits: [[NSAttributedString alloc]
      initWithString:@("Enabled Features: " YASS_APP_FEATURES)
      attributes: @{NSForegroundColorAttributeName : [NSColor labelColor],
        NSFontAttributeName: [NSFont systemFontOfSize:[NSFont labelFontSize]],
        NSParagraphStyleAttributeName:paragraphStyle}],
  };
  [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:dict];
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

- (IBAction)OnQuitMenuClicked:(id)sender {
  [[NSApplication sharedApplication] terminate:nil];
}

- (enum YASSState)getState {
  return state_;
}

- (NSString*)getStatus {
  std::ostringstream ss;
  if (state_ == STARTED) {
    NSString *prefixMessage = NSLocalizedString(@"CONNECTED_WITH_CONNS", @"Connected with conns: ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    NSString *prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else {
    NSString *prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << worker_.GetRemoteDomain();
  }

  return gurl_base::SysUTF8ToNSString(ss.str());
}

- (void)OnStart:(BOOL)quiet {
  state_ = STARTING;
  if ([self SaveConfig] == FALSE) {
    [self OnStartFailed:"Invalid Config"];
    return;
  }

  absl::AnyInvocable<void(asio::error_code)> callback;
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
  worker_.Start(std::move(callback));
}

- (void)OnStop:(BOOL)quiet {
  state_ = STOPPING;

  absl::AnyInvocable<void()> callback;
  if (!quiet) {
    callback = [=]() {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self OnStopped];
      });
    };
  }
  worker_.Stop(std::move(callback));
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

- (BOOL)SaveConfig {
  YassViewController* viewController =
      (YassViewController*)
          NSApplication.sharedApplication.mainWindow.contentViewController;
  auto server_host = gurl_base::SysNSStringToUTF8(viewController.serverHost.stringValue);
  auto server_port = StringToInteger(gurl_base::SysNSStringToUTF8(viewController.serverPort.stringValue));
  auto username = gurl_base::SysNSStringToUTF8(viewController.username.stringValue);
  auto password = gurl_base::SysNSStringToUTF8(viewController.password.stringValue);
  auto method_string =
      gurl_base::SysNSStringToUTF8(viewController.cipherMethod.stringValue);
  auto method = to_cipher_method(method_string);
  auto local_host = gurl_base::SysNSStringToUTF8(viewController.localHost.stringValue);
  auto local_port = StringToInteger(gurl_base::SysNSStringToUTF8(viewController.localPort.stringValue));
  auto connect_timeout = StringToInteger(gurl_base::SysNSStringToUTF8(viewController.timeout.stringValue));

  if (method == CRYPTO_INVALID || !server_port.has_value() ||
      !connect_timeout.has_value() || !local_port.has_value()) {
    LOG(WARNING) << "invalid options";
    return FALSE;
  }

  absl::SetFlag(&FLAGS_server_host, server_host);
  absl::SetFlag(&FLAGS_server_port, server_port.value());
  absl::SetFlag(&FLAGS_username, username);
  absl::SetFlag(&FLAGS_password, password);
  absl::SetFlag(&FLAGS_method, method);
  absl::SetFlag(&FLAGS_local_host, local_host);
  absl::SetFlag(&FLAGS_local_port, local_port.value());
  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout.value());

  return TRUE;
}

@end
