// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#ifndef YASS_IOS_UTILS
#define YASS_IOS_UTILS

#include <string>
#include <string_view>
#include <vector>

bool connectedToNetwork();
std::string serializeTelemetryJson(uint64_t total_rx_bytes, uint64_t total_tx_bytes);
bool parseTelemetryJson(std::string_view resp, uint64_t* total_rx_bytes, uint64_t* total_tx_bytes);

constexpr char kAppMessageGetTelemetry[] = "__get_telemetry";

constexpr char kServerHostFieldName[] = "server_host";
constexpr char kServerPortFieldName[] = "server_port";
constexpr char kUsernameFieldName[] = "username";
constexpr char kPasswordFieldName[] = "password";
constexpr char kMethodStringFieldName[] = "method_string";
constexpr char kConnectTimeoutFieldName[] = "connect_timeout";

#endif  //  YASS_IOS_UTILS
