// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#import "ios/YassViewController.h"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "cli/cli_connection_stats.hpp"
#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "ios/YassAppDelegate.h"

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

@interface YassViewController () <UIPickerViewDataSource, UIPickerViewDelegate>
- (NSString*)getStatusMessage;
@end

@implementation YassViewController {
  NSTimer* refresh_timer_;
  uint64_t last_sync_time_;
  uint64_t last_rx_bytes_;
  uint64_t last_tx_bytes_;
  uint64_t rx_rate_;
  uint64_t tx_rate_;
  NSArray* cipher_methods_;
  NSString* current_cipher_method_;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  cipher_methods_ = @[
#define XX(num, name, string) @string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  ];
  current_cipher_method_ = nil;
  [self.cipherMethod setDelegate:self];
  [self.cipherMethod setDataSource:self];
  [self.cipherMethod reloadAllComponents];

  [self LoadChanges];
  [self UpdateStatusBar];
  [self.startButton setEnabled:TRUE];
  [self.stopButton setEnabled:FALSE];
}

- (void)viewWillAppear {
  [self refreshTimerExceeded];
  [self.view.window center];
}

- (NSString*)getCipher {
  return current_cipher_method_;
}

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView {
  return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component {
  return [cipher_methods_ count];
}

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component {
  return [cipher_methods_ objectAtIndex:row];
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component {
  current_cipher_method_ = [cipher_methods_ objectAtIndex:row];
}

- (IBAction)OnStartButtonClicked:(id)sender {
  [self OnStart];
}

- (IBAction)OnStopButtonClicked:(id)sender {
  [self OnStop];
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
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
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
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
  [self.stopButton setEnabled:FALSE];
  [appDelegate OnStop:FALSE];
}

- (void)Started {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:FALSE];
  [self.serverPort setUserInteractionEnabled:FALSE];
  [self.username setUserInteractionEnabled:FALSE];
  [self.password setUserInteractionEnabled:FALSE];
  [self.cipherMethod setUserInteractionEnabled:FALSE];
  [self.timeout setUserInteractionEnabled:FALSE];
  [self.stopButton setEnabled:TRUE];
}

- (void)StartFailed {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:TRUE];
  [self.serverPort setUserInteractionEnabled:TRUE];
  [self.username setUserInteractionEnabled:TRUE];
  [self.password setUserInteractionEnabled:TRUE];
  [self.cipherMethod setUserInteractionEnabled:TRUE];
  [self.timeout setUserInteractionEnabled:TRUE];
  [self.startButton setEnabled:TRUE];
}

- (void)Stopped {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:TRUE];
  [self.serverPort setUserInteractionEnabled:TRUE];
  [self.username setUserInteractionEnabled:TRUE];
  [self.password setUserInteractionEnabled:TRUE];
  [self.cipherMethod setUserInteractionEnabled:TRUE];
  [self.timeout setUserInteractionEnabled:TRUE];
  [self.startButton setEnabled:TRUE];
}

- (NSString*)getStatusMessage {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
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
  self.status.text = [self getStatusMessage];
}

- (void)LoadChanges {
  self.serverHost.text =
      gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_server_host));
  self.serverPort.text =
    gurl_base::SysUTF8ToNSString(std::to_string(absl::GetFlag(FLAGS_server_port)));
  self.username.text = gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_username));
  self.password.text = gurl_base::SysUTF8ToNSString(absl::GetFlag(FLAGS_password));

  auto cipherMethod = absl::GetFlag(FLAGS_method).method;
  NSUInteger row = [cipher_methods_ indexOfObject:gurl_base::SysUTF8ToNSString(to_cipher_method_str(cipherMethod))];
  if (row != NSNotFound) {
    [self.cipherMethod selectRow:row inComponent:0 animated:NO];
  }

  self.timeout.text =
    gurl_base::SysUTF8ToNSString(std::to_string(absl::GetFlag(FLAGS_connect_timeout)));
}

@end
