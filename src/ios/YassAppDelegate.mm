// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#import "ios/YassAppDelegate.h"

#import <NetworkExtension/NetworkExtension.h>

#include <pthread.h>
#include <stdexcept>
#include <string>

#include <absl/flags/flag.h>
#include <base/strings/sys_string_conversions.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "ios/YassViewController.h"
#include "version.h"
#include "feature.h"
#include "config/config.hpp"
#include "ios/utils.h"

@interface YassAppDelegate ()
- (std::string)SaveConfig;
- (void)OnStarted;
- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*)vpn_manager;
- (void)OnStartInstanceFailed:(NSError* _Nullable)error;
- (void)OnStartFailed:(std::string)error_msg;
- (void)OnStopped;
@end

@implementation YassAppDelegate {
  enum YASSState state_;
  NSString* status_;
  std::string error_msg_;
  NETunnelProviderManager *vpn_manager_;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey,id> *)launchOptions {
  state_ = STOPPED;
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didChangeVpnStatus:) name:NEVPNStatusDidChangeNotification object:nil];
  return YES;
}

#pragma mark - UISceneSession lifecycle

- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options {
  // Called when a new scene session is being created.
  // Use this method to select a configuration to create the new scene with.
  return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}


- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
  // Called when the user discards a scene session.
  // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
  // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
}

- (enum YASSState)getState {
  return state_;
}

- (NSString*)getStatus {
  std::ostringstream ss;
  if (state_ == STARTED) {
    NSString *prefixMessage = NSLocalizedString(@"CONNECTED_WITH_CONNS", @"Connected with conns: ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << gurl_base::SysNSStringToUTF8(status_);
  } else if (state_ == START_FAILED) {
    NSString *prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else {
    NSString *prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << absl::GetFlag(FLAGS_server_host);
  }

  return gurl_base::SysUTF8ToNSString(ss.str());
}

- (void)OnStart:(BOOL)quiet {
  state_ = STARTING;
  if (!connectedToNetwork()) {
    NSString *message = NSLocalizedString(@"NETWORK_UNREACHABLE", @"Network unreachable");
    [self OnStartFailed:gurl_base::SysNSStringToUTF8(message)];
    return;
  }
  auto err_msg = [self SaveConfig];
  if (!err_msg.empty()) {
    [self OnStartFailed:err_msg];
    return;
  }

  [self OnStarted];
}

- (void)OnStop:(BOOL)quiet {
  state_ = STOPPING;

  if (vpn_manager_ != nil) {
    [vpn_manager_.connection stopVPNTunnel];
    vpn_manager_ = nil;
  }

  [self OnStopped];
}

- (void)OnStarted {
  state_ = STARTED;
  config::SaveConfig();

  [NETunnelProviderManager loadAllFromPreferencesWithCompletionHandler:^(NSArray<NETunnelProviderManager *> * _Nullable managers, NSError * _Nullable error) {
    if (error) {
      [self OnStartInstanceFailed: error];
      return;
    }
    NETunnelProviderManager* vpn_manager;
    if ([managers count] == 0) {
      vpn_manager = [[NETunnelProviderManager alloc] init];
    } else {
      vpn_manager = managers[0];
#if 0
      // Compare the configuration with new one
      // Remove old one once the configuration is changed (to prevent stall configuration).
      NETunnelProviderProtocol* tunnelProtocol = (NETunnelProviderProtocol*)vpn_manager.protocolConfiguration;

      YassViewController* viewController =
          (YassViewController*)
              UIApplication.sharedApplication.keyWindow.rootViewController;

      NSDictionary* new_configuration = @{
        @"server_host": viewController.serverHost.text,
        @"server_port": viewController.serverPort.text,
        @"username": viewController.username.text,
        @"password": viewController.password.text,
        @"method_string": [viewController getCipher],
        @"connect_timeout": viewController.timeout.text,
      };

      if (![new_configuration isEqualToDictionary:tunnelProtocol.providerConfiguration]) {
        [vpn_manager removeFromPreferencesWithCompletionHandler:^(NSError * _Nullable error) {
          if (error) {
            [self OnStartInstanceFailed: error];
            return;
          }
          [self OnStartSaveAndLoadInstance:vpn_manager];
        }];
        return;
      }
#endif
    }
    [self OnStartSaveAndLoadInstance:vpn_manager];
  }];
}

- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*) vpn_manager {
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

  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;

  tunnelProtocol.providerConfiguration = @{
    @"server_host": viewController.serverHost.text,
    @"server_port": viewController.serverPort.text,
    @"username": viewController.username.text,
    @"password": viewController.password.text,
    @"method_string": [viewController getCipher],
    @"connect_timeout": viewController.timeout.text,
  };
  tunnelProtocol.username = @"";
  tunnelProtocol.identityDataPassword = @"";
  vpn_manager.protocolConfiguration = tunnelProtocol;
  vpn_manager.localizedDescription = @"YASS";
  vpn_manager.enabled = TRUE;

  [vpn_manager saveToPreferencesWithCompletionHandler:^(NSError * _Nullable error) {
    if (error != nil) {
      [self OnStartInstanceFailed: error];
      return;
    }
    [vpn_manager loadFromPreferencesWithCompletionHandler:^(NSError * _Nullable error) {
      if (error != nil) {
        [self OnStartInstanceFailed: error];
        return;
      }
      vpn_manager_ = vpn_manager;
      BOOL ret = [vpn_manager.connection startVPNTunnelAndReturnError:&error];
      if (ret == TRUE) {
        [self didChangeVpnStatus: nil];

        YassViewController* viewController =
            (YassViewController*)
                UIApplication.sharedApplication.keyWindow.rootViewController;
        [viewController Started];
      } else {
        vpn_manager_ = nil;
        [self OnStartInstanceFailed:error];
      }
    }];
  }];
}

- (void)OnStartInstanceFailed:(NSError* _Nullable)error {
  DCHECK_EQ(vpn_manager_, nil);
  std::string err_msg = gurl_base::SysNSStringToUTF8([error localizedDescription]);
  [self OnStop:true];
  [self OnStartFailed:err_msg];
}

#pragma mark - Notification
- (void)didChangeVpnStatus:(NSNotification *)notification {
  NEVPNStatus status = vpn_manager_.connection.status;

  switch (status) {
    case NEVPNStatusConnecting:
      NSLog(@"Connecting...");
      status_ = NSLocalizedString(@"CONNECTING", @"Connecting");
      break;
    case NEVPNStatusConnected:
      NSLog(@"Connected...");
      status_ = NSLocalizedString(@"CONNECTED", @"Connected");
      break;
    case NEVPNStatusDisconnecting:
      NSLog(@"Disconnecting...");
      status_ = NSLocalizedString(@"DISCONNECTING", @"Disconnecting");
      break;
    case NEVPNStatusDisconnected:
      NSLog(@"Disconnected...");
      status_ = NSLocalizedString(@"DISCONNECTED", @"Disconnected");
      break;
    case NEVPNStatusInvalid:
      NSLog(@"Invalid");
      status_ = NSLocalizedString(@"INVALID", @"Invalid");
      break;
    case NEVPNStatusReasserting:
      NSLog(@"Reasserting...");
      status_ = NSLocalizedString(@"REASSERTING", @"Reasserting");
      break;
    default:
      NSLog(@"Unknown status... %@", @(status));
      status_ = NSLocalizedString(@"UNKNOWN", @"Unknown");
      break;
  }
  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;
  [viewController UpdateStatusBar];
}


- (void)OnStartFailed:(std::string)error_msg {
  state_ = START_FAILED;

  error_msg_ = error_msg;
  LOG(ERROR) << "worker failed due to: " << error_msg_;
  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;
  [viewController StartFailed];

  UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Start Failed" message:@(error_msg.c_str()) preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action) {}];
  [alert addAction:action];
  [viewController presentViewController:alert animated:YES completion:nil];
}

- (void)OnStopped {
  state_ = STOPPED;
  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;
  [viewController Stopped];
}

- (std::string)SaveConfig {
  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;
  auto server_host = gurl_base::SysNSStringToUTF8(viewController.serverHost.text);
  auto server_port = gurl_base::SysNSStringToUTF8(viewController.serverPort.text);
  auto username = gurl_base::SysNSStringToUTF8(viewController.username.text);
  auto password = gurl_base::SysNSStringToUTF8(viewController.password.text);
  auto method_string = gurl_base::SysNSStringToUTF8([viewController getCipher]);
  auto connect_timeout = gurl_base::SysNSStringToUTF8(viewController.timeout.text);

  return config::ReadConfigFromArgument(server_host, "" /*server_sni*/, server_port,
                                        username, password, method_string,
                                        "127.0.0.1", "0",
                                        connect_timeout);
}

@end
