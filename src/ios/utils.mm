// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "ios/utils.h"
#include "core/logging.hpp"

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <SystemConfiguration/SCNetworkReachability.h>

std::vector<std::string> GetIpAddress() {
  struct ifaddrs *ifa, *ifaddr;
  int ret = getifaddrs(&ifaddr);
  if (ret != 0) {
    PLOG(INFO) << "getifaddrs failed";
    return {};
  }
  std::vector<std::string> result, result_v6;
  for (ifa = ifaddr;ifa; ifa = ifa->ifa_next) {
    // ignoring p2p ip
    if (ifa->ifa_flags & IFF_POINTOPOINT) {
      continue;
    }
    if ((ifa->ifa_flags & (IFF_UP|IFF_RUNNING|IFF_LOOPBACK)) ==
        (IFF_UP|IFF_RUNNING)) {
      if (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family ==
          AF_INET6) {
         int s;
         char host[NI_MAXHOST] = {};

         s = getnameinfo(ifa->ifa_addr, ifa->ifa_addr->sa_len, host,
                         NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
         if (s == 0) {
           LOG(INFO) << "GetIpAddress: interface: " << ifa->ifa_name << " ip address: " << host;
           if (ifa->ifa_addr->sa_family == AF_INET6) {
             result_v6.push_back(host);
           } else {
             result.push_back(host);
           }
         }
      }
    }
  }
  freeifaddrs(ifaddr);
  result.insert(result.end(), result_v6.begin(), result_v6.end());
  return result;
}

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

