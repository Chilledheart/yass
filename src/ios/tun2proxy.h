// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef YASS_IOS_TUN2PROXY
#define YASS_IOS_TUN2PROXY

#import <Foundation/Foundation.h>

@class NEPacketTunnelFlow;
@class NEPacket;

void Tun2Proxy_Init(NEPacketTunnelFlow *flow);
void Tun2Proxy_ForwardReadPackets(NSArray<NEPacket *> *packets);
void Tun2Proxy_Destroy();

#endif //  YASS_IOS_TUN2PROXY
