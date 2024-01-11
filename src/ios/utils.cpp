// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "ios/utils.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <SystemConfiguration/SCNetworkReachability.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool connectedToNetwork() {
  // Create zero addy
  struct sockaddr_in zeroAddress;
  bzero(&zeroAddress, sizeof(zeroAddress));
  zeroAddress.sin_len = sizeof(zeroAddress);
  zeroAddress.sin_family = AF_INET;

  // Recover reachability flags
  SCNetworkReachabilityRef defaultRouteReachability = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&zeroAddress);
  SCNetworkReachabilityFlags flags;

  Boolean didRetrieveFlags = SCNetworkReachabilityGetFlags(defaultRouteReachability, &flags);
  CFRelease(defaultRouteReachability);

  if (!didRetrieveFlags)
  {
    return false;
  }

  Boolean isReachable = flags & kSCNetworkFlagsReachable;
  Boolean needsConnection = flags & kSCNetworkFlagsConnectionRequired;

  return (isReachable && !needsConnection) ? true : false;
}

std::string serializeTelemetryJson(uint64_t rx_bytes, uint64_t tx_bytes) {
  json j;
  j["total_rx_bytes"] = rx_bytes;
  j["total_tx_bytes"] = tx_bytes;
  return j.dump(4);
}

bool parseTelemetryJson(std::string_view resp, uint64_t *rx_bytes, uint64_t *tx_bytes) {
  auto root = json::parse(resp, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return false;
  }
  *rx_bytes = 0;
  if (root.contains("total_rx_bytes") && root["total_rx_bytes"].is_number_unsigned()) {
    *rx_bytes = root["total_rx_bytes"].get<uint64_t>();
  }
  *tx_bytes = 0;
  if (root.contains("total_tx_bytes") && root["total_tx_bytes"].is_number_unsigned()) {
    *tx_bytes = root["total_tx_bytes"].get<uint64_t>();
  }
  return true;
}
