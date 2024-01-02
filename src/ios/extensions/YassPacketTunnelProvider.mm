// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#import "YassPacketTunnelProvider.h"

#include "ios/tun2proxy.h"

#include <atomic>
#include <thread>

@implementation YassPacketTunnelProvider {
  std::atomic_bool stopped_;
  std::unique_ptr<std::thread> worker_thread_;
  struct Tun2Proxy_InitContext *context_;
}

- (void)startTunnelWithOptions:(NSDictionary *)options completionHandler:(void (^)(NSError *))completionHandler {
  stopped_ = false;
  // TODO get local server and port from self.protocolConfiguration
  context_ = Tun2Proxy_Init(self.packetFlow, "socks5://127.0.0.1:3000", 1500, 0, true);
  if (!context_) {
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass" code:200
                      userInfo:@{@"Error reason": @"tun2proxy init failure"}]);
    return;
  }
  worker_thread_ = std::make_unique<std::thread>([]{
    Tun2Proxy_Run(context_);
  });
  [self readPackets];
  completionHandler(nil);
}

- (void)readPackets {
  __weak YassPacketTunnelProvider *weakSelf = self;
  if (stopped_) {
    return;
  }
  [self.packetFlow readPacketObjectsWithCompletionHandler:^(NSArray<NEPacket *> *packets) {
    {
      __typeof__(self) strongSelf = weakSelf;
      Tun2Proxy_ForwardReadPackets(context_, packets);
    }
    [weakSelf readPackets];
  }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason completionHandler:(void (^)(void))completionHandler {
  stopped_ = true;
  Tun2Proxy_Destroy();
  worker_thread_->join();
  worker_thread_.reset();
  completionHandler();
}

- (void)handleAppMessage:(NSData *)messageData completionHandler:(void (^)(NSData *))completionHandler {
  // Add code here to handle the message.
}

- (void)sleepWithCompletionHandler:(void (^)(void))completionHandler {
  // Add code here to get ready to sleep.
  completionHandler();
}

- (void)wake {
  // Add code here to wake up.
}

@end
