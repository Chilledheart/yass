// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#import "mac/YassAppDelegate.h"

#include "cli/cli_worker.hpp"

#include <stdexcept>
#include <string>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "feature.h"
#include "gui_variant.h"
#include "mac/OptionViewController.h"
#include "mac/YassViewController.h"
#include "mac/YassWindowController.h"
#include "mac/utils.h"
#include "version.h"

@interface YassAppDelegate ()
- (std::string)SaveConfig;
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
  SetDockIconStyle(false);

  if (![NSApp isActive]) {
    [NSApp activateIgnoringOtherApps:YES];
  }
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  LOG(WARNING) << "Application exiting";
  [self OnStop:TRUE];
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}

- (IBAction)OnAboutMenuClicked:(id)sender {
  NSMutableParagraphStyle* paragraphStyle = [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* attributes = @{
    NSForegroundColorAttributeName : [NSColor labelColor],
    NSFontAttributeName : [NSFont systemFontOfSize:[NSFont labelFontSize]],
    NSParagraphStyleAttributeName : paragraphStyle
  };

  NSAttributedString* enabled_features =
      [[NSAttributedString alloc] initWithString:@("Enabled Features: " YASS_APP_FEATURES "\n") attributes:attributes];
  NSAttributedString* gui_variant = [[NSAttributedString alloc] initWithString:@("GUI Variant: " YASS_GUI_FLAVOUR)
                                                                    attributes:attributes];

  NSMutableAttributedString* credits = [[NSMutableAttributedString alloc] init];
  [credits appendAttributedString:enabled_features];
  [credits appendAttributedString:gui_variant];

  NSDictionary<NSAboutPanelOptionKey, id>* dict = @{
    NSAboutPanelOptionApplicationName : @(YASS_APP_PRODUCT_NAME),
    NSAboutPanelOptionApplicationVersion : @(YASS_APP_PRODUCT_VERSION),
    NSAboutPanelOptionVersion : @(YASS_APP_LAST_CHANGE),
    NSAboutPanelOptionCredits : credits,
  };
  [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:dict];
}

- (IBAction)OnOptionMenuClicked:(id)sender {
  NSStoryboard* storyboard = [NSStoryboard storyboardWithName:@"Main" bundle:nil];
  OptionViewController* optionViewController =
      (OptionViewController*)[storyboard instantiateControllerWithIdentifier:@"OptionViewController"];
  YassViewController* viewController =
      (YassViewController*)NSApplication.sharedApplication.mainWindow.contentViewController;
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
    NSString* prefixMessage = NSLocalizedString(@"CONNECTED", @"Connected");
    ss << SysNSStringToUTF8(prefixMessage) << ":";
  } else if (state_ == STARTING) {
    ss << SysNSStringToUTF8(NSLocalizedString(@"CONNECTING", @"Connecting"));
  } else if (state_ == START_FAILED) {
    NSString* prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else if (state_ == STOPPING) {
    ss << SysNSStringToUTF8(NSLocalizedString(@"DISCONNECTING", @"Disconnecting"));
  } else {
    NSString* prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << SysNSStringToUTF8(prefixMessage) << worker_.GetRemoteDomain();
  }

  return SysUTF8ToNSString(ss.str());
}

- (void)OnStart {
  state_ = STARTING;
  auto err_msg = [self SaveConfig];
  if (!err_msg.empty()) {
    [self OnStartFailed:err_msg];
    return;
  }

  absl::AnyInvocable<void(asio::error_code)> callback;
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

  YassWindowController* windowController = [YassWindowController instance];
  [windowController Started];
}

- (void)OnStartFailed:(std::string)error_msg {
  state_ = START_FAILED;

  error_msg_ = error_msg;
  YassWindowController* windowController = [YassWindowController instance];
  [windowController StartFailed];
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @(error_msg.c_str());
  alert.icon = [NSImage imageNamed:NSImageNameCaution];
  alert.alertStyle = NSAlertStyleWarning;
  [alert runModal];
}

- (void)OnStopped {
  state_ = STOPPED;
  YassWindowController* windowController = [YassWindowController instance];
  [windowController Stopped];
}

- (std::string)SaveConfig {
  YassViewController* viewController = [YassViewController instance];

  auto server_host = SysNSStringToUTF8(viewController.serverHost.stringValue);
  auto server_sni = SysNSStringToUTF8(viewController.serverSNI.stringValue);
  auto server_port = SysNSStringToUTF8(viewController.serverPort.stringValue);
  auto username = SysNSStringToUTF8(viewController.username.stringValue);
  auto password = SysNSStringToUTF8(viewController.password.stringValue);
  auto method_string = SysNSStringToUTF8(viewController.cipherMethod.stringValue);
  auto local_host = SysNSStringToUTF8(viewController.localHost.stringValue);
  auto local_port = SysNSStringToUTF8(viewController.localPort.stringValue);
  auto doh_url = SysNSStringToUTF8(viewController.dohURL.stringValue);
  auto dot_host = SysNSStringToUTF8(viewController.dotHost.stringValue);
  auto limit_rate = SysNSStringToUTF8(viewController.limitRate.stringValue);
  auto connect_timeout = SysNSStringToUTF8(viewController.timeout.stringValue);

  return config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method_string,
                                        local_host, local_port, doh_url, dot_host, limit_rate, connect_timeout);
}

@end
