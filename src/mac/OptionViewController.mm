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
  // TODO remove these entries
  [self.connectTimeout setEnabled:NO];
  [self.tcpUserTimeout setEnabled:NO];
  [self.tcpSoLingerTimeout setEnabled:NO];
  [self.tcpSendBuffer setEnabled:NO];
  [self.tcpRecevieBuffer setEnabled:NO];

  [self.tcpKeepAlive
      setState:(absl::GetFlag(FLAGS_tcp_keep_alive) ? NSControlStateValueOn : NSControlStateValueOff)];
  self.tcpKeepAliveCnt.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
  self.tcpKeepAliveIdleTimeout.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
  self.tcpKeepAliveInterval.intValue = absl::GetFlag(FLAGS_tcp_keep_alive_interval);
}

- (void)viewDidDisappear {
  [NSApp stopModalWithCode:NSModalResponseCancel];
}

- (IBAction)OnOkButtonClicked:(id)sender {
  absl::SetFlag(&FLAGS_connect_timeout, self.connectTimeout.intValue);

  absl::SetFlag(&FLAGS_tcp_keep_alive, self.tcpKeepAlive.state == NSControlStateValueOn);
  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, self.tcpKeepAliveCnt.intValue);
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, self.tcpKeepAliveIdleTimeout.intValue);
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, self.tcpKeepAliveInterval.intValue);
  [self dismissViewController:self];
}

- (IBAction)OnCancelButtonClicked:(id)sender {
  [self dismissViewController:self];
}

@end
