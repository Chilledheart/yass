// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "ios/utils.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/socket.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string serializeTelemetryJson(uint64_t total_rx_bytes, uint64_t total_tx_bytes) {
  json j;
  j["total_rx_bytes"] = total_rx_bytes;
  j["total_tx_bytes"] = total_tx_bytes;
  return j.dump(4);
}

bool parseTelemetryJson(std::string_view resp, uint64_t* total_rx_bytes, uint64_t* total_tx_bytes) {
  auto root = json::parse(resp, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return false;
  }
  *total_rx_bytes = 0;
  if (root.contains("total_rx_bytes") && root["total_rx_bytes"].is_number_unsigned()) {
    *total_rx_bytes = root["total_rx_bytes"].get<uint64_t>();
  }
  *total_tx_bytes = 0;
  if (root.contains("total_tx_bytes") && root["total_tx_bytes"].is_number_unsigned()) {
    *total_tx_bytes = root["total_tx_bytes"].get<uint64_t>();
  }
  return true;
}
