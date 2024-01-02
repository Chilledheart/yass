// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef YASS_IOS_TUN2PROXY
#define YASS_IOS_TUN2PROXY

#import <Foundation/Foundation.h>
#include <string>

@class NEPacketTunnelFlow;
@class NEPacket;
struct Tun2Proxy_InitContext;

Tun2Proxy_InitContext* Tun2Proxy_Init(NEPacketTunnelFlow *flow,
                                      const std::string& proxy_url,
                                      int tun_mtu,
                                      int log_level,
                                      bool dns_over_tcp);
int Tun2Proxy_Run(Tun2Proxy_InitContext* context);
void Tun2Proxy_ForwardReadPackets(Tun2Proxy_InitContext* context, NSArray<NEPacket *> *packets);
void Tun2Proxy_Destroy(Tun2Proxy_InitContext* context);

#endif //  YASS_IOS_TUN2PROXY
