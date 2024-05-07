// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#import "ios/YassAppDelegate.h"

#import <NetworkExtension/NetworkExtension.h>

#include <stdexcept>
#include <string>
#include <string_view>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "feature.h"
#include "ios/YassViewController.h"
#include "ios/utils.h"
#include "version.h"

@interface YassAppDelegate ()
- (std::string)SaveConfig;
- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*)vpn_manager;
- (void)OnStarting;
- (void)OnStarted;
- (void)OnStartFailedWithNSError:(NSError* _Nullable)error;
- (void)OnStartFailed:(std::string)error_msg;
- (void)OnStopping;
- (void)OnStopped;
@end

@implementation YassAppDelegate {
  enum YASSState state_;
  NSString* status_;
  std::string error_msg_;
  NETunnelProviderManager* vpn_manager_;
  NSTimer* refresh_timer_;

  NSString* server_host_;
  NSString* server_port_;
  NSString* username_;
  NSString* password_;
  NSString* method_string_;
  NSString* doh_url_;
  NSString* dot_host_;
  NSString* connect_timeout_;
  BOOL enable_post_quantum_kyber_;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
  state_ = STOPPED;
  [self didDefaultsChanged:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(didDefaultsChanged:)
                                               name:NSUserDefaultsDidChangeNotification
                                             object:nil];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(didChangeVpnStatus:)
                                               name:NEVPNStatusDidChangeNotification
                                             object:nil];
  return YES;
}

#pragma mark - UISceneSession lifecycle

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  // Called when a new scene session is being created.
  // Use this method to select a configuration to create the new scene with.
  return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}

- (void)application:(UIApplication*)application didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
  // Called when the user discards a scene session.
  // If any sessions were discarded while the application was not running, this will be called shortly after
  // application:didFinishLaunchingWithOptions. Use this method to release any resources that were specific to the
  // discarded scenes, as they will not return.
}

#pragma mark - Application

- (void)reloadState {
  [NETunnelProviderManager loadAllFromPreferencesWithCompletionHandler:^(
                               NSArray<NETunnelProviderManager*>* _Nullable managers, NSError* _Nullable error) {
    if (error) {
      vpn_manager_ = nil;
      [self didChangeVpnStatus:nil];
      return;
    }
    NETunnelProviderManager* vpn_manager;
    if ([managers count] == 0) {
      vpn_manager_ = nil;
      [self didChangeVpnStatus:nil];
      return;
    } else {
      vpn_manager = managers[0];
    }
    vpn_manager_ = vpn_manager.enabled ? vpn_manager : nil;
    [self didChangeVpnStatus:nil];
  }];
}

- (enum YASSState)getState {
  return state_;
}

- (YassViewController*)getRootViewController {
  NSSet<UIScene*>* scenes = [[UIApplication sharedApplication] connectedScenes];
  for (UIScene* scene : scenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) {
      continue;
    }
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    UIWindow* keyWindow = nil;
    if (@available(iOS 15.0, *)) {
      keyWindow = windowScene.keyWindow;
    } else {
      NSArray<UIWindow*>* windows = windowScene.windows;
      for (UIWindow* window : windows) {
        if (window.isKeyWindow) {
          keyWindow = window;
          break;
        }
      }
    }
    UIViewController* viewController = keyWindow.rootViewController;
    if (![viewController isKindOfClass:[YassViewController class]]) {
      continue;
    }
    return (YassViewController*)viewController;
  }
  return nil;
}

- (NSString*)getStatus {
  std::ostringstream ss;
  if (state_ == STARTED) {
    ss << SysNSStringToUTF8(status_) << ":";
  } else if (state_ == STARTING) {
    ss << SysNSStringToUTF8(NSLocalizedString(@"CONNECTING", @"Connecting"));
  } else if (state_ == START_FAILED) {
    NSString* prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else if (state_ == STOPPING) {
    ss << SysNSStringToUTF8(NSLocalizedString(@"DISCONNECTING", @"Disconnecting"));
  } else {
    NSString* prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << SysNSStringToUTF8(prefixMessage) << absl::GetFlag(FLAGS_server_host);
  }

  return SysUTF8ToNSString(ss.str());
}

- (void)OnStart:(BOOL)quiet {
  [self OnStarting];

  if (!connectedToNetwork()) {
    NSString* message = NSLocalizedString(@"NETWORK_UNREACHABLE", @"Network unreachable");
    [self OnStartFailed:SysNSStringToUTF8(message)];
    return;
  }
  auto err_msg = [self SaveConfig];
  if (!err_msg.empty()) {
    [self OnStartFailed:err_msg];
    return;
  }

  config::SaveConfig();

  [NETunnelProviderManager loadAllFromPreferencesWithCompletionHandler:^(
                               NSArray<NETunnelProviderManager*>* _Nullable managers, NSError* _Nullable error) {
    if (error != nil) {
      [self OnStartFailedWithNSError:error];
      return;
    }
    NETunnelProviderManager* vpn_manager;
    if ([managers count] == 0) {
      vpn_manager = [[NETunnelProviderManager alloc] init];
    } else {
      vpn_manager = managers[0];
    }
    [self OnStartSaveAndLoadInstance:vpn_manager];
  }];
}

- (void)OnStop:(BOOL)quiet {
  if (vpn_manager_ != nil) {
    // state will reflect in NEVPNStatusDidChangeNotification
    [vpn_manager_.connection stopVPNTunnel];
    [self didChangeVpnStatus:nil];
  } else {
    [self OnStopped];
  }
}

- (void)FetchTelemetryData {
  if (vpn_manager_ != nil && state_ == STARTED) {
    NETunnelProviderSession* session = (NETunnelProviderSession*)vpn_manager_.connection;
    NSData* requestData = [@(kAppMessageGetTelemetry) dataUsingEncoding:NSUTF8StringEncoding];
    NSError* error;
    [session sendProviderMessage:requestData
                     returnError:&error
                 responseHandler:^(NSData* _Nullable responseData) {
                   if (!responseData) {
                     return;
                   }
                   std::string_view response((const char*)responseData.bytes, responseData.length);
                   uint64_t total_rx_bytes;
                   uint64_t total_tx_bytes;
                   if (!parseTelemetryJson(response, &total_rx_bytes, &total_tx_bytes)) {
                     LOG(WARNING) << "telemetry: Invalid response: " << response;
                     return;
                   }
                   dispatch_async(dispatch_get_main_queue(), ^{
                     // non atomic write
                     self.total_rx_bytes = total_rx_bytes;
                     self.total_tx_bytes = total_tx_bytes;
                     YassViewController* viewController = [self getRootViewController];
                     [viewController UpdateStatusBar];
                   });
                 }];
    if (error) {
      std::string err_msg = SysNSStringToUTF8([error localizedDescription]);
      LOG(WARNING) << "telemetry: send Request failed: " << err_msg;
    }
  }
}

- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*)vpn_manager {
  NETunnelProviderProtocol* tunnelProtocol;
  if (!vpn_manager.protocolConfiguration) {
    tunnelProtocol = [[NETunnelProviderProtocol alloc] init];
  } else {
    tunnelProtocol = (NETunnelProviderProtocol*)vpn_manager.protocolConfiguration;
  }

  tunnelProtocol.providerBundleIdentifier = @"it.gui.ios.yass.network-extension";
  tunnelProtocol.disconnectOnSleep = FALSE;
  tunnelProtocol.serverAddress = @"Yet Another Shadow Socket";
#if 0
  if (@available(iOS 15.1, *)) {
    LOG(INFO) << "Activating includeAllNetworks";
    tunnelProtocol.includeAllNetworks = TRUE;
    tunnelProtocol.excludeLocalNetworks = TRUE;
    if (@available(iOS 16.4, *)) {
      // By default, APNs are excluded from the VPN tunnel on 16.4 and later
      tunnelProtocol.excludeAPNs = FALSE;
    }
  }
#endif

  tunnelProtocol.providerConfiguration = @{
    @(kServerHostFieldName) : server_host_,
    @(kServerPortFieldName) : server_port_,
    @(kUsernameFieldName) : username_,
    @(kPasswordFieldName) : password_,
    @(kMethodStringFieldName) : method_string_,
    @(kDoHURLFieldName) : doh_url_,
    @(kDoTHostFieldName) : dot_host_,
    @(kConnectTimeoutFieldName) : connect_timeout_,
    @(kEnablePostQuantumKyberKey) : @(enable_post_quantum_kyber_),
  };
  tunnelProtocol.username = @"";
  tunnelProtocol.identityDataPassword = @"";
  vpn_manager.protocolConfiguration = tunnelProtocol;
  vpn_manager.localizedDescription = @"YASS";
  vpn_manager.enabled = TRUE;

  [vpn_manager saveToPreferencesWithCompletionHandler:^(NSError* _Nullable error) {
    if (error != nil) {
      [self OnStartFailedWithNSError:error];
      return;
    }
    [vpn_manager loadFromPreferencesWithCompletionHandler:^(NSError* _Nullable error) {
      if (error != nil) {
        [self OnStartFailedWithNSError:error];
        return;
      }
      BOOL ret = [vpn_manager.connection startVPNTunnelAndReturnError:&error];
      if (ret == TRUE) {
        vpn_manager_ = vpn_manager;
        // state will reflect in NEVPNStatusDidChangeNotification
        [self didChangeVpnStatus:nil];
      } else {
        [self OnStartFailedWithNSError:error];
      }
    }];
  }];
}

#pragma mark - Notification

- (void)didDefaultsChanged:(NSNotification*)notification {
  // Use standard one for data that is only for Host App.
  // Use suiteName for data that you want to share between Extension and Host App.
  enable_post_quantum_kyber_ = [[NSUserDefaults standardUserDefaults] boolForKey:@(kEnablePostQuantumKyberKey)];
  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, enable_post_quantum_kyber_);
}

- (void)didChangeVpnStatus:(NSNotification*)notification {
  if (!vpn_manager_) {
    status_ = NSLocalizedString(@"DISCONNECTED", @"Disconnected");
    [self OnStopped];
    return;
  }
  NEVPNStatus status = vpn_manager_.connection.status;
  switch (status) {
    case NEVPNStatusConnecting:
      NSLog(@"Connecting...");
      status_ = NSLocalizedString(@"CONNECTING", @"Connecting");
      [self OnStarting];
      break;
    case NEVPNStatusConnected:
      NSLog(@"Connected...");
      status_ = NSLocalizedString(@"CONNECTED", @"Connected");
      [self OnStarted];
      break;
    case NEVPNStatusDisconnecting:
      NSLog(@"Disconnecting...");
      status_ = NSLocalizedString(@"DISCONNECTING", @"Disconnecting");
      [self OnStopping];
      break;
    case NEVPNStatusDisconnected:
      NSLog(@"Disconnected...");
      status_ = NSLocalizedString(@"DISCONNECTED", @"Disconnected");
      [self OnStopped];
      break;
    case NEVPNStatusInvalid:
      NSLog(@"Invalid");
      status_ = NSLocalizedString(@"INVALID", @"Invalid");
      [self OnStopped];
      break;
    case NEVPNStatusReasserting:
      NSLog(@"Reasserting...");
      status_ = NSLocalizedString(@"REASSERTING", @"Reasserting");
      [self OnStopped];
      break;
    default:
      NSLog(@"Unknown status... %@", @(status));
      status_ = NSLocalizedString(@"UNKNOWN", @"Unknown");
      [self OnStopped];
      break;
  }
}

- (void)OnStarting {
  state_ = STARTING;

  YassViewController* viewController = [self getRootViewController];
  [viewController Starting];
}

- (void)OnStarted {
  state_ = STARTED;

  YassViewController* viewController = [self getRootViewController];
  [viewController Started];

  refresh_timer_ = [NSTimer scheduledTimerWithTimeInterval:NSTimeInterval(1.0)
                                                    target:self
                                                  selector:@selector(FetchTelemetryData)
                                                  userInfo:nil
                                                   repeats:YES];
}

- (void)OnStartFailedWithNSError:(NSError* _Nullable)error {
  std::string err_msg = SysNSStringToUTF8([error localizedDescription]);
  [self OnStartFailed:err_msg];
}

- (void)OnStartFailed:(std::string)error_msg {
  state_ = START_FAILED;
  error_msg_ = error_msg;  // required by viewController

  YassViewController* viewController = [self getRootViewController];
  [viewController StartFailed];

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:NSLocalizedString(@"START_FAILED", @"Start Failed")
                                          message:@(error_msg.c_str())
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action = [UIAlertAction actionWithTitle:NSLocalizedString(@"OK", @"OK")
                                                   style:UIAlertActionStyleDefault
                                                 handler:^(UIAlertAction* action){
                                                 }];
  [alert addAction:action];
  [viewController presentViewController:alert animated:YES completion:nil];
}

- (void)OnStopping {
  state_ = STOPPING;

  YassViewController* viewController = [self getRootViewController];
  [viewController Stopping];
}

- (void)OnStopped {
  state_ = STOPPED;

  YassViewController* viewController = [self getRootViewController];
  [viewController Stopped];

  [refresh_timer_ invalidate];
  refresh_timer_ = nil;
}

- (std::string)SaveConfig {
  YassViewController* viewController = [self getRootViewController];
  server_host_ = viewController.serverHost.text;
  server_port_ = viewController.serverPort.text;
  username_ = viewController.username.text;
  password_ = viewController.password.text;
  method_string_ = viewController.currentCiphermethod;
  doh_url_ = viewController.dohURL.text;
  dot_host_ = viewController.dotHost.text;
  connect_timeout_ = viewController.timeout.text;

  auto server_host = SysNSStringToUTF8(server_host_);
  auto server_port = SysNSStringToUTF8(server_port_);
  auto username = SysNSStringToUTF8(username_);
  auto password = SysNSStringToUTF8(password_);
  auto method_string = SysNSStringToUTF8(method_string_);
  auto doh_url = SysNSStringToUTF8(doh_url_);
  auto dot_host = SysNSStringToUTF8(dot_host_);
  auto connect_timeout = SysNSStringToUTF8(connect_timeout_);

  return config::ReadConfigFromArgument(server_host, "" /*server_sni*/, server_port, username, password, method_string,
                                        "127.0.0.1", "0", doh_url, dot_host, connect_timeout);
}

@end
