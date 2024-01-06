// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

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
#include "ios/utils.h"

@interface YassViewController () <UIPickerViewDataSource, UIPickerViewDelegate, UITextFieldDelegate>
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
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  ];
  current_cipher_method_ = nil;
  [self.cipherMethod setDelegate:self];
  [self.cipherMethod setDataSource:self];
  [self.cipherMethod reloadAllComponents];
  [self.serverHost setDelegate:self];
  [self.serverPort setDelegate:self];
  [self.username setDelegate:self];
  [self.password setDelegate:self];
  [self.timeout setDelegate:self];

  [self LoadChanges];
  [self UpdateStatusBar];
  [self.startButton setEnabled:TRUE];
  [self.stopButton setEnabled:FALSE];
  (void)connectedToNetwork();
}

- (void)viewWillAppear {
  refresh_timer_ =
      [NSTimer scheduledTimerWithTimeInterval:NSTimeInterval(0.2)
                                       target:self
                                     selector:@selector(UpdateStatusBar)
                                     userInfo:nil
                                      repeats:YES];
  [self.view.window center];
}

- (void)viewWillDisappear:(BOOL)animated {
  [refresh_timer_ invalidate];
  refresh_timer_ = nil;
  [super viewWillDisappear:animated];
}

- (NSString*)getCipher {
  return current_cipher_method_;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  [textField resignFirstResponder];
  return NO;
}

- (BOOL)textFieldShouldEndEditing:(UITextField *)textField {
  if (textField == self.serverHost) {
    if (textField.text.length > _POSIX_HOST_NAME_MAX) {
      return NO;
    }
  }
  if (textField == self.serverPort) {
    auto port = StringToInteger(gurl_base::SysNSStringToUTF8(textField.text));
    return port.has_value() && port.value() > 0 && port.value() < 65536 ? YES : NO;
  }
  if (textField == self.timeout) {
    auto port = StringToInteger(gurl_base::SysNSStringToUTF8(textField.text));
    return port.has_value() && port.value() >= 0 ? YES : NO;
  }
  return YES;
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

- (void)UpdateStatusBar {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
  self.status.text = [appDelegate getStatus];
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
    current_cipher_method_ = gurl_base::SysUTF8ToNSString(to_cipher_method_str(cipherMethod));
    [self.cipherMethod selectRow:row inComponent:0 animated:NO];
  }

  self.timeout.text =
    gurl_base::SysUTF8ToNSString(std::to_string(absl::GetFlag(FLAGS_connect_timeout)));
}

@end
