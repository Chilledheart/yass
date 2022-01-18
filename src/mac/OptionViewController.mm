// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import "mac/OptionViewController.h"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

@interface OptionViewController ()

@end

@implementation OptionViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.connectTimeout.intValue = absl::GetFlag(FLAGS_connect_timeout);
  self.tcpUserTimeout.intValue = absl::GetFlag(FLAGS_tcp_user_timeout);
  self.tcpSoLingerTimeout.intValue = absl::GetFlag(FLAGS_so_linger_timeout);
  self.tcpSendBuffer.intValue = absl::GetFlag(FLAGS_so_snd_buffer);
  self.tcpRecevieBuffer.intValue = absl::GetFlag(FLAGS_so_rcv_buffer);
}

- (void)viewDidDisappear {
  [NSApp stopModalWithCode:NSModalResponseCancel];
}

- (IBAction)OnOkButtonClicked:(id)sender {
  absl::SetFlag(&FLAGS_connect_timeout, self.connectTimeout.intValue);
  absl::SetFlag(&FLAGS_tcp_user_timeout, self.tcpUserTimeout.intValue);
  absl::SetFlag(&FLAGS_so_linger_timeout, self.tcpSoLingerTimeout.intValue);
  absl::SetFlag(&FLAGS_so_snd_buffer, self.tcpSendBuffer.intValue);
  absl::SetFlag(&FLAGS_so_rcv_buffer, self.tcpRecevieBuffer.intValue);
  [self dismissViewController:self];
}

- (IBAction)OnCancelButtonClicked:(id)sender {
  [self dismissViewController:self];
}

@end
