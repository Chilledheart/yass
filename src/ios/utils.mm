// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "ios/utils.h"
#include "core/logging.hpp"

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <SystemConfiguration/SCNetworkReachability.h>

bool connectedToNetwork() {
  // Create zero addy
  struct sockaddr_in zeroAddress;
  bzero(&zeroAddress, sizeof(zeroAddress));
  zeroAddress.sin_len = sizeof(zeroAddress);
  zeroAddress.sin_family = AF_INET;

  // Recover reachability flags
  SCNetworkReachabilityRef defaultRouteReachability = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&zeroAddress);
  SCNetworkReachabilityFlags flags;

  BOOL didRetrieveFlags = SCNetworkReachabilityGetFlags(defaultRouteReachability, &flags);
  CFRelease(defaultRouteReachability);

  if (!didRetrieveFlags)
  {
    return false;
  }

  BOOL isReachable = flags & kSCNetworkFlagsReachable;
  BOOL needsConnection = flags & kSCNetworkFlagsConnectionRequired;

  return (isReachable && !needsConnection) ? true : false;
}

