// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#import "ios/YassAppDelegate.h"

#import <NetworkExtension/NetworkExtension.h>

#include "cli/cli_worker.hpp"

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

@interface YassAppDelegate ()
- (void)SaveConfig;
- (void)OnStarted;
- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*)vpn_manager;
- (void)OnStartInstanceFailed:(NSError* _Nullable)error;
- (void)OnStartFailed:(std::string)error_msg;
- (void)OnStopped;
@end

@implementation YassAppDelegate {
  enum YASSState state_;
  std::string error_msg_;
  Worker worker_;
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
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    NSString *prefixMessage = NSLocalizedString(@"FAILED_TO_CONNECT_DUE_TO", @"Failed to connect due to ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << error_msg_.c_str();
  } else {
    NSString *prefixMessage = NSLocalizedString(@"DISCONNECTED_WITH", @"Disconnected with ");
    ss << gurl_base::SysNSStringToUTF8(prefixMessage) << worker_.GetRemoteDomain();
  }

  return gurl_base::SysUTF8ToNSString(ss.str());
}

- (void)OnStart:(BOOL)quiet {
  state_ = STARTING;
  [self SaveConfig];

  absl::AnyInvocable<void(asio::error_code)> callback;
  if (!quiet) {
    callback = [=](asio::error_code ec) {
      bool successed = false;
      std::string msg;

      if (ec) {
        msg = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      dispatch_async(dispatch_get_main_queue(), ^{
        if (successed) {
          [self OnStarted];
        } else {
          [self OnStartFailed:msg];
        }
      });
    };
  }
  worker_.Start(std::move(callback));
}

- (void)OnStop:(BOOL)quiet {
  state_ = STOPPING;

  if (vpn_manager_ != nil) {
    [vpn_manager_.connection stopVPNTunnel];
    vpn_manager_ = nil;
  }

  absl::AnyInvocable<void()> callback;
  if (!quiet) {
    callback = [=]() {
      dispatch_async(dispatch_get_main_queue(), ^{
        [self OnStopped];
      });
    };
  }
  worker_.Stop(std::move(callback));
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
    }

    [self OnStartSaveAndLoadInstance:vpn_manager];
  }];
}

- (void)OnStartSaveAndLoadInstance:(NETunnelProviderManager*) vpn_manager  {
  NETunnelProviderProtocol* tunnelProtocol = [[NETunnelProviderProtocol alloc] init];
  tunnelProtocol.providerBundleIdentifier = @"it.gui.ios.yass.network-extension";
  tunnelProtocol.disconnectOnSleep = FALSE;
  tunnelProtocol.serverAddress = @"Yet Another Shadow Socket";
  if (@available(iOS 15.1, *)) {
    LOG(INFO) << "Activating includeAllNetworks";
    tunnelProtocol.includeAllNetworks = TRUE;
    tunnelProtocol.excludeLocalNetworks = TRUE;
    if (@available(iOS 16.4, *)) {
      // By default, APNs are excluded from the VPN tunnel on 16.4 and later
      tunnelProtocol.excludeAPNs = false;
    }
  }

  tunnelProtocol.providerConfiguration = @{
    @"ip": @"127.0.0.1",
    @"port": @3000,
  };
  tunnelProtocol.username = @"";
  tunnelProtocol.identityDataPassword = @"";
  vpn_manager.protocolConfiguration = tunnelProtocol;
  vpn_manager.localizedDescription = @"YASS VPN";
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
      break;
    case NEVPNStatusConnected:
      NSLog(@"Connected...");
      break;
    case NEVPNStatusDisconnecting:
      NSLog(@"Disconnecting...");
      break;
    case NEVPNStatusDisconnected:
      NSLog(@"Disconnected...");
      break;
    case NEVPNStatusInvalid:
      NSLog(@"Invalid");
      break;
    case NEVPNStatusReasserting:
      NSLog(@"Reasserting...");
      break;
    default:
      NSLog(@"Unknown status... %@", @(status));
      break;
  }
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

- (void)SaveConfig {
  YassViewController* viewController =
      (YassViewController*)
          UIApplication.sharedApplication.keyWindow.rootViewController;
  auto server_host = gurl_base::SysNSStringToUTF8(viewController.serverHost.text);
  auto server_port =
    StringToInteger(gurl_base::SysNSStringToUTF8(viewController.serverPort.text));
  auto username = gurl_base::SysNSStringToUTF8(viewController.username.text);
  auto password = gurl_base::SysNSStringToUTF8(viewController.password.text);
  auto method_string = gurl_base::SysNSStringToUTF8([viewController getCipher]);
  auto method = to_cipher_method(method_string);
  auto connect_timeout =
    StringToInteger(gurl_base::SysNSStringToUTF8(viewController.timeout.text));

  if (method == CRYPTO_INVALID || !server_port.has_value() ||
      !connect_timeout.has_value()) {
    LOG(WARNING) << "invalid options";
    return;
  }

  absl::SetFlag(&FLAGS_server_host, server_host);
  absl::SetFlag(&FLAGS_server_port, server_port.value());
  absl::SetFlag(&FLAGS_username, username);
  absl::SetFlag(&FLAGS_password, password);
  absl::SetFlag(&FLAGS_method, method);
  absl::SetFlag(&FLAGS_local_host, "0.0.0.0");
  absl::SetFlag(&FLAGS_local_port, 3000);
  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout.value());
}

@end
