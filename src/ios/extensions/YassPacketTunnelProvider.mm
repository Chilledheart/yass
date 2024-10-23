// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#import "YassPacketTunnelProvider.h"

#include <pthread.h>
#include <signal.h>
#include <atomic>
#include <thread>

#include <absl/strings/str_format.h>

#include "cli/cli_connection_stats.hpp"
#include "cli/cli_worker.hpp"
#include "config/config.hpp"
#include "core/utils.hpp"
#include "ios/utils.h"
#include "tun2proxy.h"

using namespace std::string_literals;

namespace config {
const ProgramType pType = YASS_CLIENT_GUI;
}  // namespace config

static constexpr const int DEFAULT_MTU = 1500;
static const char PRIVATE_VLAN4_CLIENT[] = "172.19.0.1";
static const char PRIVATE_VLAN4_GATEWAY[] = "172.19.0.2";
static const char PRIVATE_VLAN6_CLIENT[] = "fdfe:dcba:9876::1";
static const char PRIVATE_VLAN6_GATEWAY[] = "fdfe:dcba:9876::2";
static constexpr const uint32_t kYieldConcurrencyOfConnections = 12u;

@implementation YassPacketTunnelProvider {
  std::atomic_bool stopped_;
  std::once_flag init_flag_;
  std::unique_ptr<Worker> worker_;
  std::unique_ptr<std::thread> tun2proxy_thread_;
  struct Tun2Proxy_InitContext* context_;
}

- (void)startTunnelWithOptions:(NSDictionary*)options completionHandler:(void (^)(NSError*))completionHandler {
  stopped_ = false;
  context_ = nullptr;
  // setup signal handler
  signal(SIGPIPE, SIG_IGN);

  /* Block SIGPIPE in all threads, this can happen if a thread calls write on
     a closed pipe. */
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask failed");
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass"
                                          code:200
                                      userInfo:@{@"Error reason" : @(strerror(errno))}]);
    return;
  }
  SetExecutablePath("UNKNOWN.ext");

  NETunnelProviderProtocol* protocolConfiguration = (NETunnelProviderProtocol*)self.protocolConfiguration;
  NSDictionary* dict = protocolConfiguration.providerConfiguration;
  auto server_host = SysNSStringToUTF8(dict[@(kServerHostFieldName)]);
  auto server_sni = SysNSStringToUTF8(dict[@(kServerSNIFieldName)]);
  auto server_port = SysNSStringToUTF8(dict[@(kServerPortFieldName)]);
  auto username = SysNSStringToUTF8(dict[@(kUsernameFieldName)]);
  auto password = SysNSStringToUTF8(dict[@(kPasswordFieldName)]);
  constexpr std::string_view local_host = "127.0.0.1";
  constexpr std::string_view local_port = "0";
  auto method_string = SysNSStringToUTF8(dict[@(kMethodStringFieldName)]);
  auto doh_url = SysNSStringToUTF8(dict[@(kDoHURLFieldName)]);
  auto dot_host = SysNSStringToUTF8(dict[@(kDoTHostFieldName)]);
  auto limit_rate = SysNSStringToUTF8(dict[@(kLimitRateFieldName)]);
  auto connect_timeout = SysNSStringToUTF8(dict[@(kConnectTimeoutFieldName)]);
  auto enable_post_quantum_kyber = [dict[@(kEnablePostQuantumKyberKey)] boolValue];

  auto err_msg = config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method_string,
                                                local_host, local_port, doh_url, dot_host, limit_rate, connect_timeout);
  if (!err_msg.empty()) {
    completionHandler([NSError errorWithDomain:@"it.gui.ios.yass"
                                          code:200
                                      userInfo:@{@"Error reason" : @(err_msg.c_str())}]);
    return;
  }

  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, enable_post_quantum_kyber);

  absl::AnyInvocable<void(asio::error_code)> callback = [self, completionHandler](asio::error_code ec) {
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
      BOOL ret = [self startTunnelWithOptionsOnCallback:completionHandler];

      if (!ret) {
        [self stopTunnelWithReason:NEProviderStopReasonConfigurationFailed
                 completionHandler:^() {
                   completionHandler([NSError errorWithDomain:@"it.gui.ios.yass"
                                                         code:200
                                                     userInfo:@{@"Error reason" : @"tunnel init failure"}]);
                 }];
        return;
      }
    } else {
      completionHandler([NSError errorWithDomain:@"it.gui.ios.yass"
                                            code:200
                                        userInfo:@{@"Error reason" : @(err_msg.c_str())}]);
    }
  };
  std::call_once(init_flag_, [self]() { worker_ = std::make_unique<Worker>(); });
  worker_->Start(std::move(callback));
}

- (BOOL)startTunnelWithOptionsOnCallback:(void (^)(NSError*))completionHandler {
  NETunnelProviderProtocol* protocolConfiguration = (NETunnelProviderProtocol*)self.protocolConfiguration;

  NSDictionary* dict = protocolConfiguration.providerConfiguration;
  NSString* local_host = @"127.0.0.1";
  int local_port = worker_->GetLocalPort();

  auto ips_v4 = worker_->GetRemoteIpsV4();
  NSMutableArray* remote_ips_v4 = [[NSMutableArray alloc] init];
  for (const auto& ip_v4 : ips_v4) {
    [remote_ips_v4 addObject:@(ip_v4.c_str())];
  }
  auto ips_v6 = worker_->GetRemoteIpsV6();
  NSMutableArray* remote_ips_v6 = [[NSMutableArray alloc] init];
  for (const auto& ip_v6 : ips_v6) {
    [remote_ips_v6 addObject:@(ip_v6.c_str())];
  }

  NSString* remoteIp = nullptr;
  if ([remote_ips_v4 count]) {
    remoteIp = remote_ips_v4[0];
  } else if ([remote_ips_v6 count]) {
    remoteIp = remote_ips_v6[0];
  } else {
    NSLog(@"tun2proxy inited with empty remote ip addresses");
    return FALSE;
  }

  std::string proxy_url = absl::StrFormat("socks5://%s:%d", SysNSStringToUTF8(local_host), local_port);

  context_ = Tun2Proxy_Init(self.packetFlow, proxy_url, DEFAULT_MTU, 0, true);
  if (!context_) {
    NSLog(@"tun2proxy inited with empty tun2proxy context");
    return FALSE;
  }
  NSLog(@"tun2proxy inited with %s remote ip %@ mtu %d dns_proxy %s", proxy_url.c_str(), remoteIp, DEFAULT_MTU, "true");
  Tun2Proxy_InitContext* context = context_;
  tun2proxy_thread_ = std::make_unique<std::thread>([context] {
    if (!SetCurrentThreadName("tun2proxy"s)) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
    NSLog(@"tun2proxy thread begin");
    Tun2Proxy_Run(context);
    NSLog(@"tun2proxy thread done");
  });

  NEPacketTunnelNetworkSettings* tunnelNetworkSettings =
      [[NEPacketTunnelNetworkSettings alloc] initWithTunnelRemoteAddress:remoteIp];
  tunnelNetworkSettings.MTU = [NSNumber numberWithInteger:DEFAULT_MTU];

  NSLog(@"tunnel: init ip v4 route");
  // Setting IPv4 Route
  tunnelNetworkSettings.IPv4Settings =
      [[NEIPv4Settings alloc] initWithAddresses:[NSArray arrayWithObjects:@(PRIVATE_VLAN4_CLIENT), nil]
                                    subnetMasks:[NSArray arrayWithObjects:@("255.255.255.224"), nil]];
  NEIPv4Route* ipv4route = [NEIPv4Route defaultRoute];
  ipv4route.gatewayAddress = @(PRIVATE_VLAN4_GATEWAY);
  tunnelNetworkSettings.IPv4Settings.includedRoutes = @[ ipv4route ];

  NSMutableArray* excludeRoutes = [[NSMutableArray alloc] init];
  for (NSString* ip : remote_ips_v4) {
    NEIPv4Route* excludeRoute = [[NEIPv4Route alloc] initWithDestinationAddress:ip subnetMask:@"255.255.255.255"];
    [excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv4Settings.excludedRoutes = excludeRoutes;

  // Save some memory
  NSLog(@"tunnel: init ip v6 route");
  // Setting IPv6 Route
  tunnelNetworkSettings.IPv6Settings =
      [[NEIPv6Settings alloc] initWithAddresses:[NSArray arrayWithObjects:@(PRIVATE_VLAN6_CLIENT), nil]
                           networkPrefixLengths:[NSArray arrayWithObjects:@126, nil]];
  NEIPv6Route* ipv6route = [NEIPv6Route defaultRoute];
  ipv6route.gatewayAddress = @(PRIVATE_VLAN6_GATEWAY);
  tunnelNetworkSettings.IPv6Settings.includedRoutes = @[ ipv6route ];

  NSMutableArray* ipv6_excludeRoutes = [[NSMutableArray alloc] init];
  for (NSString* ip : remote_ips_v6) {
    NEIPv6Route* excludeRoute = [[NEIPv6Route alloc] initWithDestinationAddress:ip networkPrefixLength:@126];
    [ipv6_excludeRoutes addObject:excludeRoute];
  }
  tunnelNetworkSettings.IPv6Settings.excludedRoutes = ipv6_excludeRoutes;

  // Setting DNS Settings
  NEDNSSettings* dnsSettings =
      [[NEDNSSettings alloc] initWithServers:[NSArray arrayWithObjects:@(PRIVATE_VLAN4_CLIENT), nil]];
  dnsSettings.matchDomains = [NSArray arrayWithObjects:@"", nil];
  tunnelNetworkSettings.DNSSettings = dnsSettings;

  // Setting Proxy Settings
#if 0
  NSLog(@"tunnel: disable proxy settings");
#else
  NSLog(@"tunnel: init proxy settings");
  NEProxySettings* proxySettings = [[NEProxySettings alloc] init];
  proxySettings.HTTPServer = [[NEProxyServer alloc] initWithAddress:local_host port:local_port];
  proxySettings.HTTPEnabled = TRUE;
  proxySettings.HTTPSServer = [[NEProxyServer alloc] initWithAddress:local_host port:local_port];
  proxySettings.HTTPSEnabled = TRUE;
  proxySettings.exceptionList = [NSArray arrayWithObjects:@"127.0.0.1", @"localhost", @"*.local", nil];
  tunnelNetworkSettings.proxySettings = proxySettings;
#endif

  __weak YassPacketTunnelProvider* weakSelf = self;
  [self setTunnelNetworkSettings:tunnelNetworkSettings
               completionHandler:^(NSError* _Nullable error) {
                 if (error) {
                   __typeof__(self) strongSelf = weakSelf;
                   if (strongSelf == nil) {
                     completionHandler(error);
                     return;
                   }
                   [strongSelf stopTunnelWithReason:NEProviderStopReasonConfigurationFailed
                                  completionHandler:^() {
                                    completionHandler(error);
                                  }];
                   return;
                 }
                 NSLog(@"tunnel: start");
                 completionHandler(nil);
               }];
  [self readPackets];
  return TRUE;
}

- (void)readPackets {
  __weak YassPacketTunnelProvider* weakSelf = self;
  [self.packetFlow readPacketObjectsWithCompletionHandler:^(NSArray<NEPacket*>* packets) {
    __typeof__(self) strongSelf = weakSelf;
    if (strongSelf == nil) {
      return;
    }
    if (strongSelf->stopped_) {
      return;
    }
    Tun2Proxy_ForwardReadPackets(strongSelf->context_, packets);
    if (worker_->currentConnections() > kYieldConcurrencyOfConnections) {
      NSLog(@"tunnel: about to yield after %zu connection", worker_->currentConnections());
      sched_yield();  // wait for up to 10ms
    }
    [strongSelf readPackets];
  }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason completionHandler:(void (^)(void))completionHandler {
  NSLog(@"tunnel: stop with reason %ld", reason);
  if (stopped_) {
    completionHandler();
    return;
  }
  stopped_ = true;
  if (!worker_) {
    // nothing to do, just leave
    NSLog(@"tunnel: stopped with empty worker");
    completionHandler();
    return;
  }
  absl::AnyInvocable<void()> callback = [self, completionHandler]() {
    NSLog(@"worker stopped");
    if (tun2proxy_thread_) {
      Tun2Proxy_Shutdown(context_);
      NSLog(@"tun2proxy shutdown");
      tun2proxy_thread_->join();
      tun2proxy_thread_.reset();
    }
    Tun2Proxy_Destroy(context_);
    NSLog(@"tun2proxy destroyed");
    context_ = nullptr;
    completionHandler();
  };
  worker_->Stop(std::move(callback));
}

- (void)handleAppMessage:(NSData*)messageData completionHandler:(void (^)(NSData*))completionHandler {
  NSString* request = [[NSString alloc] initWithData:messageData encoding:NSUTF8StringEncoding];
  if ([request isEqualToString:@(kAppMessageGetTelemetry)]) {
    std::string response = serializeTelemetryJson(net::cli::total_rx_bytes, net::cli::total_tx_bytes);
    NSData* responseData = [NSData dataWithBytes:response.c_str() length:response.size()];
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
