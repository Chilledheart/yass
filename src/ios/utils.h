// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#ifndef YASS_IOS_UTILS
#define YASS_IOS_UTILS

#include <string>
#include <string_view>
#include <vector>

void initNetworkPathMonitor();
void deinitNetworkPathMonitor();

bool connectedToNetwork();

std::string serializeTelemetryJson(uint64_t total_rx_bytes, uint64_t total_tx_bytes);
bool parseTelemetryJson(std::string_view resp, uint64_t* total_rx_bytes, uint64_t* total_tx_bytes);

constexpr const char kAppMessageGetTelemetry[] = "__get_telemetry";

constexpr const char kServerHostFieldName[] = "server_host";
constexpr const char kServerPortFieldName[] = "server_port";
constexpr const char kUsernameFieldName[] = "username";
constexpr const char kPasswordFieldName[] = "password";
constexpr const char kMethodStringFieldName[] = "method_string";
constexpr const char kDoHURLFieldName[] = "doh_url";
constexpr const char kDoTHostFieldName[] = "dot_host";
constexpr const char kConnectTimeoutFieldName[] = "connect_timeout";

constexpr const char kEnablePostQuantumKyberKey[] = "ENABLE_POST_QUANTUM_KYBER";

#endif  //  YASS_IOS_UTILS
