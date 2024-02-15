// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#import "ios/YassViewController.h"

#include <sstream>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "ios/YassAppDelegate.h"

@interface YassViewController () <UIPickerViewDataSource, UIPickerViewDelegate, UITextFieldDelegate>
@end

@implementation YassViewController {
  NSArray* cipher_methods_;
  uint64_t last_sync_time_;
  uint64_t last_rx_bytes_;
  uint64_t last_tx_bytes_;
  uint64_t rx_rate_;
  uint64_t tx_rate_;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  cipher_methods_ = @[
#define XX(num, name, string) @string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  ];
  self.currentCiphermethod = @(CRYPTO_HTTP2_STR);
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
}

- (void)viewWillAppear:(BOOL)animated {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
  [appDelegate reloadState];
  [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillShowNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification * _Nonnull notification) {
    NSDictionary *userInfo = notification.userInfo;
    NSTimeInterval duration = [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
    CGRect keyboardEndFrame = [self.view convertRect:[userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue] toView:self.view.window];

    CGRect windowFrame = [self.view convertRect:self.view.window.frame toView:self.view.window];
    CGFloat heightOffset = (windowFrame.size.height - keyboardEndFrame.origin.y) - self.view.frame.origin.y;

    self.bottomConstraint.constant = heightOffset + 20;

    [UIView animateWithDuration:duration
                     animations:^{
      [self.view layoutIfNeeded];
    }];
  }];

  [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillHideNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification * _Nonnull notification) {
    NSDictionary *userInfo = notification.userInfo;
    NSTimeInterval duration = [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
    self.bottomConstraint.constant =  20;
    [UIView animateWithDuration:duration
                     animations:^{
      [self.view layoutIfNeeded];
    }];
  }];
  [super viewWillAppear:animated];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  if (textField == self.serverHost) {
    [textField resignFirstResponder];
    [self.serverPort becomeFirstResponder];
  } else if (textField == self.serverPort) {
    [textField resignFirstResponder];
    [self.username becomeFirstResponder];
  } else if (textField == self.username) {
    [textField resignFirstResponder];
    [self.password becomeFirstResponder];
  } else if (textField == self.password) {
    [textField resignFirstResponder];
    [self.timeout becomeFirstResponder];
  } else if (textField == self.timeout) {
    [textField resignFirstResponder];
    [self OnStart];
  }

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
  self.currentCiphermethod = [cipher_methods_ objectAtIndex:row];
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
  [appDelegate OnStart:FALSE];
}

- (void)OnStop {
  YassAppDelegate* appDelegate =
      (YassAppDelegate*)UIApplication.sharedApplication.delegate;
  [appDelegate OnStop:FALSE];
}

- (void)Starting {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:FALSE];
  [self.serverPort setUserInteractionEnabled:FALSE];
  [self.username setUserInteractionEnabled:FALSE];
  [self.password setUserInteractionEnabled:FALSE];
  [self.cipherMethod setUserInteractionEnabled:FALSE];
  [self.timeout setUserInteractionEnabled:FALSE];
  [self.startButton setEnabled:FALSE];
  [self.stopButton setEnabled:FALSE];
}

- (void)Started {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:FALSE];
  [self.serverPort setUserInteractionEnabled:FALSE];
  [self.username setUserInteractionEnabled:FALSE];
  [self.password setUserInteractionEnabled:FALSE];
  [self.cipherMethod setUserInteractionEnabled:FALSE];
  [self.timeout setUserInteractionEnabled:FALSE];
  [self.startButton setEnabled:FALSE];
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
  [self.stopButton setEnabled:FALSE];
}

- (void)Stopping {
  [self UpdateStatusBar];
  [self.serverHost setUserInteractionEnabled:FALSE];
  [self.serverPort setUserInteractionEnabled:FALSE];
  [self.username setUserInteractionEnabled:FALSE];
  [self.password setUserInteractionEnabled:FALSE];
  [self.cipherMethod setUserInteractionEnabled:FALSE];
  [self.timeout setUserInteractionEnabled:FALSE];
  [self.startButton setEnabled:FALSE];
  [self.stopButton setEnabled:FALSE];
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
  [self.stopButton setEnabled:FALSE];
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
    uint64_t rx_bytes = appDelegate.total_rx_bytes;
    uint64_t tx_bytes = appDelegate.total_tx_bytes;
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
  HumanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  message = NSLocalizedString(@"RXRATE", @" rx rate: ");
  ss << gurl_base::SysNSStringToUTF8(message);
  HumanReadableByteCountBin(&ss, tx_rate_);
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
    self.currentCiphermethod = gurl_base::SysUTF8ToNSString(to_cipher_method_str(cipherMethod));
    [self.cipherMethod selectRow:row inComponent:0 animated:NO];
  }

  self.timeout.text =
    gurl_base::SysUTF8ToNSString(std::to_string(absl::GetFlag(FLAGS_connect_timeout)));
}

@end
