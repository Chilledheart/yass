// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

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

  [self.tcpKeepAlive setState:(absl::GetFlag(FLAGS_tcp_keep_alive) ? NSControlStateValueOn : NSControlStateValueOff)];
  self.tcpKeepAliveCnt.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
  self.tcpKeepAliveIdleTimeout.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
  self.tcpKeepAliveInterval.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_interval);
  [self.enablePostQuantumKyber
      setState:(absl::GetFlag(FLAGS_enable_post_quantum_kyber) ? NSControlStateValueOn : NSControlStateValueOff)];
}

- (void)viewDidDisappear {
  [NSApp stopModalWithCode:NSModalResponseCancel];
}

- (IBAction)OnOkButtonClicked:(id)sender {
  absl::SetFlag(&FLAGS_tcp_keep_alive, self.tcpKeepAlive.state == NSControlStateValueOn);
  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, self.tcpKeepAliveCnt.intValue);
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, self.tcpKeepAliveIdleTimeout.intValue);
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, self.tcpKeepAliveInterval.intValue);
  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, self.enablePostQuantumKyber.state == NSControlStateValueOn);
  config::SaveConfig();
  [self dismissViewController:self];
}

- (IBAction)OnCancelButtonClicked:(id)sender {
  [self dismissViewController:self];
}

@end
