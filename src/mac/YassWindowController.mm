// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#import "mac/YassWindowController.h"

#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "cli/cli_connection_stats.hpp"
#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "mac/YassAppDelegate.h"
#import "mac/YassViewController.h"
#include "mac/utils.h"

@interface YassWindowController ()
@end

@implementation YassWindowController {
  NSStatusItem* y_status_bar_item_;
  NSStatusItem* status_bar_item_;
  NSTimer* refresh_timer_;
  uint64_t last_sync_time_;
  uint64_t last_rx_bytes_;
  uint64_t last_tx_bytes_;
  uint64_t rx_rate_;
  uint64_t tx_rate_;
  BOOL hide_on_closed_;
}

- (void)windowDidLoad {
  [super windowDidLoad];
  [self refreshTimerExceeded];

  y_status_bar_item_ = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];

  y_status_bar_item_.button.title = @"Y";
  y_status_bar_item_.button.target = self;
  y_status_bar_item_.button.action = @selector(statusItemClicked);
  [y_status_bar_item_.button
      sendActionOn:NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown | NSEventMaskOtherMouseDown];

  status_bar_item_ = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];

  hide_on_closed_ = FALSE;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
  if (sender != self.window) {
    return TRUE;
  }
  SetDockIconStyle(true);
  hide_on_closed_ = TRUE;

  [self.window orderOut:nil];
  return FALSE;
}

- (void)statusItemClicked {
  if (hide_on_closed_) {
    SetDockIconStyle(false);
    hide_on_closed_ = FALSE;
  }

  NSWindow* window = self.window;
  [window performSelector:@selector(orderFrontRegardless) withObject:nil afterDelay:0.10];
  [window makeKeyAndOrderFront:nil];
}

- (void)UpdateStatusBar {
  status_bar_item_.button.title = [self getStatusMessage];
}

- (NSString*)getStatusMessage {
  YassAppDelegate* appDelegate = (YassAppDelegate*)NSApplication.sharedApplication.delegate;
  if ([appDelegate getState] != STARTED) {
    return [appDelegate getStatus];
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time * NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time * NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  NSString* message = [appDelegate getStatus];
  ss << gurl_base::SysNSStringToUTF8(message);
  message = NSLocalizedString(@"TXRATE", @" tx rate: ");
  ss << gurl_base::SysNSStringToUTF8(message);
  HumanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  message = NSLocalizedString(@"RXRATE", @" rx rate: ");
  ss << gurl_base::SysNSStringToUTF8(message);
  HumanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return gurl_base::SysUTF8ToNSString(ss.str());
}

- (void)refreshTimerExceeded {
  if (refresh_timer_) {
    [refresh_timer_ invalidate];
    [self UpdateStatusBar];
  }

  refresh_timer_ = [NSTimer scheduledTimerWithTimeInterval:NSTimeInterval(0.2)
                                                    target:self
                                                  selector:@selector(refreshTimerExceeded)
                                                  userInfo:nil
                                                   repeats:NO];
}

- (void)OnStart {
  YassAppDelegate* appDelegate = (YassAppDelegate*)NSApplication.sharedApplication.delegate;

  last_sync_time_ = GetMonotonicTime();
  last_rx_bytes_ = 0U;
  last_tx_bytes_ = 0U;
  cli::total_rx_bytes = 0U;
  cli::total_tx_bytes = 0U;
  [appDelegate OnStart];
}

- (void)OnStop {
  YassAppDelegate* appDelegate = (YassAppDelegate*)NSApplication.sharedApplication.delegate;
  [appDelegate OnStop:FALSE];
}

- (void)Started {
  [self UpdateStatusBar];
  YassViewController* viewController = (YassViewController*)self.contentViewController;
  [viewController Started];
}

- (void)StartFailed {
  [self UpdateStatusBar];
  YassViewController* viewController = (YassViewController*)self.contentViewController;
  [viewController StartFailed];
}

- (void)Stopped {
  [self UpdateStatusBar];
  YassViewController* viewController = (YassViewController*)self.contentViewController;
  [viewController Stopped];
}

@end
