// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#import "PacketTunnelProvider.h"

@implementation PacketTunnelProvider

- (void)startTunnelWithOptions:(NSDictionary *)options completionHandler:(void (^)(NSError *))completionHandler {
    // Add code here to start the process of connecting the tunnel.
    completionHandler(nil);
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason completionHandler:(void (^)(void))completionHandler {
    // Add code here to start the process of stopping the tunnel.
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
