// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#import "YassPacketTunnelProvider.h"

#include <atomic>
#include <thread>

#include <absl/strings/str_format.h>

#include "tun2proxy.h"
#include "config/config.hpp"
#include "cli/cli_worker.hpp"
#include "core/utils.hpp"
#include "ios/utils.h"
#include "cli/cli_connection_stats.hpp"

static constexpr int DEFAULT_MTU = 1500;
static const char PRIVATE_VLAN4_CLIENT[] = "172.19.0.1";
static const char PRIVATE_VLAN4_GATEWAY[] = "172.19.0.2";
static const char PRIVATE_VLAN6_CLIENT[] = "fdfe:dcba:9876::1";
static const char PRIVATE_VLAN6_GATEWAY[] = "fdfe:dcba:9876::2";

@implementation YassPacketTunnelProvider {
  std::atomic_bool stopped_;
  Worker worker_;
  std::unique_ptr<std::thread> tun2proxy_thread_;
  struct Tun2Proxy_InitContext *context_;
}

- (void)startTunnelWithOptions:(NSDictionary *)options completionHandler:(void (^)(NSError *))completionHandler {
  stopped_ = false;
  absl::SetFlag(&FLAGS_logtostderr, false);
#ifdef NDEBUG
  absl::SetFlag(&FLAGS_stderrthreshold, LOGGING_WARNING);
#else
  absl::SetFlag(&FLAGS_stderrthreshold, LOGGING_VERBOSE);
#endif

#if 0
  RateFlag rate(10u << 20);
  NSLog(@"tunnel: applying rate limit: %04.2lfm/s", rate.rate / 1024.0 / 1024.0);
  absl::SetFlag(&FLAGS_limit_rate, rate);

  int worker_limit = 12;
  NSLog(@"tunnel: applying concurrent limit: %d", worker_limit);
  absl::SetFlag(&FLAGS_worker_connections, worker_limit);
#endif

  NETunnelProviderProtocol *protocolConfiguration = (NETunnelProviderProtocol*)self.protocolConfiguration;
  NSDictionary* dict = protocolConfiguration.providerConfiguration;
  auto server_host = gurl_base::SysNSStringToUTF8(dict[@"server_host"]);
  auto server_port = gurl_base::SysNSStringToUTF8(dict[@"server_port"]);
  auto username = gurl_base::SysNSStringToUTF8(dict[@"username"]);
  auto password = gurl_base::SysNSStringToUTF8(dict[@"password"]);
  auto local_host = std::string("127.0.0.1");
  auto local_port = std::string("0");
  auto method_string = gurl_base::SysNSStringToUTF8(dict[@"method_string"]);
  auto connect_timeout = gurl_base::SysNSStringToUTF8(dict[@"connect_timeout"]);

  auto err_msg = config::ReadConfigFromArgument(server_host, "" /*server_sni*/, server_port,
                                                username, password, method_string,
                                                local_host, local_port, connect_timeout);
  if (!err_msg.empty()) {
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass" code:200
                      userInfo:@{@"Error reason": @(err_msg.c_str())}]);
    return;
  }

  absl::AnyInvocable<void(asio::error_code)> callback;
  callback = [=](asio::error_code ec) {
    bool successed = false;
    std::string err_msg;

    if (ec) {
      err_msg = ec.message();
      successed = false;
    } else {
      successed = true;
    }

    if (successed) {
      NSLog(@"worker started");
      [self startTunnelWithOptionsOnCallback:completionHandler];
    } else {
      completionHandler([NSError errorWithDomain:@"it.gui.ios.yass" code:200
                        userInfo:@{@"Error reason": @(err_msg.c_str())}]);
    }
  };
  cli::total_rx_bytes = 0;
  cli::total_tx_bytes = 0;
  worker_.Start(std::move(callback));
}

- (void)startTunnelWithOptionsOnCallback:(void (^)(NSError *))completionHandler {
  NETunnelProviderProtocol *protocolConfiguration = (NETunnelProviderProtocol*)self.protocolConfiguration;

  NSDictionary* dict = protocolConfiguration.providerConfiguration;
  NSString* local_host = @"127.0.0.1";
  int local_port = worker_.GetLocalPort();

  auto ips_v4 = worker_.GetRemoteIpsV4();
  NSMutableArray *remote_ips_v4 = [[NSMutableArray alloc] init];
  for (const auto & ip_v4 : ips_v4) {
    [remote_ips_v4 addObject:@(ip_v4.c_str())];
  }
  auto ips_v6 = worker_.GetRemoteIpsV6();
  NSMutableArray *remote_ips_v6 = [[NSMutableArray alloc] init];
  for (const auto & ip_v6 : ips_v6) {
    [remote_ips_v6 addObject:@(ip_v6.c_str())];
  }

  std::string proxy_url = absl::StrFormat("socks5://%s:%d",
                                          gurl_base::SysNSStringToUTF8(local_host),
                                          local_port);

  context_ = Tun2Proxy_Init(self.packetFlow, proxy_url, DEFAULT_MTU, 0, true);
  if (!context_) {
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass" code:200
                      userInfo:@{@"Error reason": @"tun2proxy init failure"}]);
    return;
  }
  NSLog(@"tun2proxy inited with %s mtu %d dns_proxy %s", proxy_url.c_str(), DEFAULT_MTU, "true");
  Tun2Proxy_InitContext *context = context_;
  tun2proxy_thread_ = std::make_unique<std::thread>([context]{
    if (!SetCurrentThreadName("tun2proxy")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
    NSLog(@"tun2proxy thread begin");
    Tun2Proxy_Run(context);
    NSLog(@"tun2proxy thread done");
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

  NSMutableArray *excludeRoutes = [[NSMutableArray alloc] init];
  for (NSString *ip : remote_ips_v4) {
    NEIPv4Route *excludeRoute = [[NEIPv4Route alloc]
      initWithDestinationAddress:ip subnetMask:@"255.255.255.255"];
    [excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv4Settings.excludedRoutes = excludeRoutes;

  // Save some memory
#if 0
  NSLog(@"tunnel: disable ip v6 route");
#else
  NSLog(@"tunnel: init ip v6 route");
  // Setting IPv6 Route
  tunnelNetworkSettings.IPv6Settings = [[NEIPv6Settings alloc]
    initWithAddresses:[NSArray arrayWithObjects:@(PRIVATE_VLAN6_CLIENT), nil]
    networkPrefixLengths:[NSArray arrayWithObjects:@126, nil]];
  NEIPv6Route *ipv6route = [NEIPv6Route defaultRoute];
  ipv6route.gatewayAddress = @(PRIVATE_VLAN6_GATEWAY);
  tunnelNetworkSettings.IPv6Settings.includedRoutes = @[ipv6route];

  NSMutableArray *ipv6_excludeRoutes = [[NSMutableArray alloc] init];
  for (NSString *ip : remote_ips_v6) {
    NEIPv6Route *excludeRoute = [[NEIPv6Route alloc]
      initWithDestinationAddress:ip networkPrefixLength:@126];
    [ipv6_excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv6Settings.excludedRoutes = ipv6_excludeRoutes;
#endif

  // Setting DNS Settings
  NEDNSSettings *dnsSettings = [[NEDNSSettings alloc]
    initWithServers: [NSArray arrayWithObjects:@(PRIVATE_VLAN4_CLIENT), nil]];
  dnsSettings.matchDomains = [NSArray arrayWithObjects:@"", nil];
  tunnelNetworkSettings.DNSSettings = dnsSettings;

  // Setting Proxy Settings
#if 0
  NSLog(@"tunnel: disable proxy settings");
#else
  NSLog(@"tunnel: init proxy settings");
  NEProxySettings *proxySettings = [[NEProxySettings alloc] init];
  proxySettings.HTTPServer = [[NEProxyServer alloc] initWithAddress:local_host port:local_port];
  proxySettings.HTTPEnabled = TRUE;
  proxySettings.HTTPSServer = [[NEProxyServer alloc] initWithAddress:local_host port:local_port];
  proxySettings.HTTPSEnabled = TRUE;
  proxySettings.exceptionList = [NSArray arrayWithObjects:@"127.0.0.1", @"localhost", @"*.local", nil];
  tunnelNetworkSettings.proxySettings = proxySettings;
#endif

  [self setTunnelNetworkSettings:tunnelNetworkSettings
               completionHandler:^(NSError * _Nullable error) {
    if (error) {
      completionHandler(error);
      return;
    }
    NSLog(@"tunnel: start");
    completionHandler(nil);
  }];
  [self readPackets];
}

- (void)readPackets {
  __weak YassPacketTunnelProvider *weakSelf = self;
  [self.packetFlow readPacketObjectsWithCompletionHandler:^(NSArray<NEPacket *> *packets) {
    __typeof__(self) strongSelf = weakSelf;
    if (strongSelf == nil) {
      return;
    }
    if (strongSelf->stopped_) {
      return;
    }
    Tun2Proxy_ForwardReadPackets(strongSelf->context_, packets);
    if (worker_.currentConnections() > 12) {
      NSLog(@"tunnel: sched_yield after %zu connection", worker_.currentConnections());
      sched_yield(); // wait for up to 10ms
    }
    [strongSelf readPackets];
  }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason completionHandler:(void (^)(void))completionHandler {
  NSLog(@"tunnel: stop with reason %ld", reason);
  stopped_ = true;
  worker_.Stop([=]{
    NSLog(@"worker stopped");
    Tun2Proxy_Destroy(context_);
    NSLog(@"tun2proxy destroyed");
    tun2proxy_thread_->join();
    tun2proxy_thread_.reset();
    completionHandler();
  });
}

- (void)handleAppMessage:(NSData *)messageData completionHandler:(void (^)(NSData *))completionHandler {
  NSString *request = [[NSString alloc] initWithData:messageData encoding:NSUTF8StringEncoding];
  if ([request isEqualToString:@(kAppMessageGetTelemetry)]) {
    std::string response = serializeTelemetryJson(cli::total_rx_bytes, cli::total_tx_bytes);
    NSData *responseData = [NSData dataWithBytes:response.c_str() length:response.size()];
    completionHandler(responseData);
  }
}

- (void)sleepWithCompletionHandler:(void (^)(void))completionHandler {
  // Add code here to get ready to sleep.
  completionHandler();
}

- (void)wake {
  // Add code here to wake up.
}

@end
