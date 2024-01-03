// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#import "YassPacketTunnelProvider.h"

#include "ios/tun2proxy.h"

#include <atomic>
#include <thread>

static constexpr int DEFAULT_MTU = 1400;
static const char PRIVATE_VLAN4_CLIENT[] = "172.19.0.1";
static const char PRIVATE_VLAN4_GATEWAY[] = "172.19.0.2";
static const char PRIVATE_VLAN6_CLIENT[] = "fdfe:dcba:9876::1";
static const char PRIVATE_VLAN6_GATEWAY[] = "fdfe:dcba:9876::2";

@implementation YassPacketTunnelProvider {
  std::atomic_bool stopped_;
  std::unique_ptr<std::thread> worker_thread_;
  struct Tun2Proxy_InitContext *context_;
}

- (void)startTunnelWithOptions:(NSDictionary *)options completionHandler:(void (^)(NSError *))completionHandler {
  stopped_ = false;

  NETunnelProviderProtocol *protocolConfiguration = (NETunnelProviderProtocol*)self.protocolConfiguration;
  NSString* ip = protocolConfiguration.providerConfiguration[@"ip"];
  NSNumber* port = protocolConfiguration.providerConfiguration[@"port"];
  NSArray* remote_ips_v4 = protocolConfiguration.providerConfiguration[@"remote_ips_v4"];
  NSArray* remote_ips_v6 = protocolConfiguration.providerConfiguration[@"remote_ips_v6"];
  context_ = Tun2Proxy_Init(self.packetFlow, "socks5://127.0.0.1:3000", DEFAULT_MTU, 0, true);
  if (!context_) {
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass" code:200
                      userInfo:@{@"Error reason": @"tun2proxy init failure"}]);
    return;
  }
  NSLog(@"tun2proxy: init");
  Tun2Proxy_InitContext *context = context_;
  worker_thread_ = std::make_unique<std::thread>([context]{
    NSLog(@"tun2proxy: worker thread begin");
    Tun2Proxy_Run(context);
    NSLog(@"tun2proxy: worker thread done");
  });

  NEPacketTunnelNetworkSettings *tunnelNetworkSettings = [[NEPacketTunnelNetworkSettings alloc] initWithTunnelRemoteAddress:remote_ips_v4[0]];
  tunnelNetworkSettings.MTU = [NSNumber numberWithInteger:DEFAULT_MTU];

  // Setting IPv4 Route
  tunnelNetworkSettings.IPv4Settings = [[NEIPv4Settings alloc]
    initWithAddresses:[NSArray arrayWithObjects:@(PRIVATE_VLAN4_CLIENT), nil]
    subnetMasks:[NSArray arrayWithObjects:@("255.255.255.224"), nil]];
  NEIPv4Route *ipv4route = [NEIPv4Route defaultRoute];
  ipv4route.gatewayAddress = @(PRIVATE_VLAN4_GATEWAY);
  tunnelNetworkSettings.IPv4Settings.includedRoutes = @[ipv4route];

  NSMutableArray *excludeRoutes = [NSMutableArray alloc] init];
  for (NSString *ip : remote_ips_v4) {
    NEIPv4Route *excludeRoute = [[NEIPv4Route alloc]
      initWithDestinationAddress:ip subnetMask:@"255.255.255.255"];
    [excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv4Settings.excludedRoutes = excludeRoutes;

  // Setting IPv6 Route
  tunnelNetworkSettings.IPv6Settings = [[NEIPv6Settings alloc]
    initWithAddresses:[NSArray arrayWithObjects:@(PRIVATE_VLAN6_CLIENT), nil]
    networkPrefixLengths:[NSArray arrayWithObjects:@126, nil]];
  NEIPv6Route *ipv6route = [NEIPv6Route defaultRoute];
  ipv6route.gatewayAddress = @(PRIVATE_VLAN6_GATEWAY);
  tunnelNetworkSettings.IPv6Settings.includedRoutes = @[ipv6route];

  NSMutableArray *excludeRoutes = [NSMutableArray alloc] init];
  for (NSString *ip : remote_ips_v6) {
    NEIPv6Route *excludeRoute = [[NEIPv6Route alloc]
      initWithDestinationAddress:ip subnetMask:@"255.255.255.255"];
    [excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv6Settings.excludedRoutes = excludeRoutes;

  // Setting DNS Settings
  NEDNSSettings *dnsSettings = [[NEDNSSettings alloc]
    initWithServers: [NSArray arrayWithObjects:@(PRIVATE_VLAN4_CLIENT), @(PRIVATE_VLAN6_CLIENT), nil]];
  dnsSettings.matchDomains = [NSArray arrayWithObjects:@"", nil];
  tunnelNetworkSettings.DNSSettings = dnsSettings;

  // Setting Proxy Settings
  NEProxySettings *proxySettings = [[NEProxySettings alloc] init];
  proxySettings.HTTPServer = [[NEProxyServer alloc] initWithAddress:ip port:port];
  proxySettings.HTTPEnabled = TRUE;
  proxySettings.HTTPSServer = [[NEProxyServer alloc] initWithAddress:ip port:port];
  proxySettings.HTTPSEnabled = TRUE;
  proxySettings.exceptionList = [NSArray arrayWithObjects:@"127.0.0.1", @"localhost", @"*.local", nil]
  tunnelNetworkSettings.proxySettings = proxySettings;

  [self setTunnelNetworkSettings:tunnelNetworkSettings
               completionHandler:^(NSError * _Nullable error) {
    if (error) {
      completionHandler(error);
      return;
    }
    NSLog(@"tun2proxy: start completion");
    completionHandler(nil);
  }];
  [self readPackets];
}

- (void)readPackets {
  __weak YassPacketTunnelProvider *weakSelf = self;
  if (stopped_) {
    return;
  }
  NSLog(@"tun2proxy: read packet begin");
  [self.packetFlow readPacketObjectsWithCompletionHandler:^(NSArray<NEPacket *> *packets) {
    NSLog(@"tun2proxy: read packet - %@", packets);
    {
      __typeof__(self) strongSelf = weakSelf;
      Tun2Proxy_ForwardReadPackets(context_, packets);
    }

    [weakSelf readPackets];
  }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason completionHandler:(void (^)(void))completionHandler {
  NSLog(@"tun2proxy: stop with reason %ld", reason);
  stopped_ = true;
  Tun2Proxy_Destroy(context_);
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
