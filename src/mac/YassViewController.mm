// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import "mac/YassViewController.h"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "cli/cli_connection_stats.hpp"
#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "mac/YassAppDelegate.h"
#include "mac/utils.h"

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

@interface YassViewController ()
- (NSString*)getStatusMessage;
@end

@implementation YassViewController {
  NSStatusItem* status_bar_item_;
  NSTimer* refresh_timer_;
  uint64_t last_sync_time_;
  uint64_t last_rx_bytes_;
  uint64_t last_tx_bytes_;
  uint64_t rx_rate_;
  uint64_t tx_rate_;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  status_bar_item_ = [[NSStatusBar systemStatusBar]
      statusItemWithLength:NSVariableStatusItemLength];

  [self.cipherMethod removeAllItems];
  NSString* methodStrings[] = {
#define XX(num, name, string) @string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  for (uint32_t i = 1; i < sizeof(methodStrings) / sizeof(methodStrings[0]);
       ++i) {
    [self.cipherMethod addItemWithObjectValue:methodStrings[i]];
  }
  self.cipherMethod.numberOfVisibleItems =
      sizeof(methodStrings) / sizeof(methodStrings[0]) - 1;

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
  status_bar_item_.button.action = @selector(OnStatusBarClicked:);
  [self refreshTimerExceeded];
  [self.view.window center];
}

- (void)setRepresentedObject:(id)representedObject {
  [super setRepresentedObject:representedObject];

  // Update the view, if already loaded.
}

- (void)viewDidDisappear {
  [[NSApplication sharedApplication] terminate:nil];
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

- (void)OnStatusBarClicked:(id)sender {
  if ([NSApp currentEvent].type == NSEventTypeLeftMouseDown) {
    NSWindow* window = self.view.window;
    if ([window isVisible])
      [window performSelector:@selector(orderFrontRegardless)
                   withObject:nil
                   afterDelay:0.05];
    [window makeKeyWindow];
  }
}

- (void)refreshTimerExceeded {
  if (refresh_timer_) {
    [refresh_timer_ invalidate];
    [self UpdateStatusBar];
  }

  refresh_timer_ =
      [NSTimer scheduledTimerWithTimeInterval:NSTimeInterval(0.2)
                                       target:self
                                     selector:@selector(refreshTimerExceeded)
                                     userInfo:nil
                                      repeats:NO];
}

- (void)OnStart {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)NSApplication.sharedApplication.delegate;
  [self.startButton setEnabled:FALSE];

  last_sync_time_ = GetMonotonicTime();
  last_rx_bytes_ = 0U;
  last_tx_bytes_ = 0U;
  cli::total_rx_bytes = 0U;
  cli::total_tx_bytes = 0U;
  [appDelegate OnStart:FALSE];
}

- (void)OnStop {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)NSApplication.sharedApplication.delegate;
  [self.stopButton setEnabled:FALSE];
  [appDelegate OnStop:FALSE];
}

- (void)Started {
  [self UpdateStatusBar];
  [self.serverHost setEditable:FALSE];
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
  [self UpdateStatusBar];
  [self.serverHost setEditable:TRUE];
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
  [self UpdateStatusBar];
  [self.serverHost setEditable:TRUE];
  [self.serverPort setEditable:TRUE];
  [self.username setEditable:TRUE];
  [self.password setEditable:TRUE];
  [self.cipherMethod setEditable:TRUE];
  [self.localHost setEditable:TRUE];
  [self.localPort setEditable:TRUE];
  [self.timeout setEditable:TRUE];
  [self.startButton setEnabled:TRUE];
}

- (NSString*)getStatusMessage {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)NSApplication.sharedApplication.delegate;
  if ([appDelegate getState] != STARTED) {
    return [appDelegate getStatus];
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time *
               NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time *
               NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  NSString *message = [appDelegate getStatus];
  ss << gurl_base::SysNSStringToUTF8(message);
  message = NSLocalizedString(@"TXRATE", @" tx rate: ");
  ss << gurl_base::SysNSStringToUTF8(message);
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  message = NSLocalizedString(@"RXRATE", @" rx rate: ");
  ss << gurl_base::SysNSStringToUTF8(message);
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return gurl_base::SysUTF8ToNSString(ss.str());
}

- (void)UpdateStatusBar {
  status_bar_item_.title = [self getStatusMessage];
}

- (void)LoadChanges {
  self.serverHost.stringValue =
      gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_server_host));
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
