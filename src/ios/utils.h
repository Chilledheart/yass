// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */
#ifndef YASS_IOS_UTILS
#define YASS_IOS_UTILS

#include <string>
#include <string_view>
#include <vector>

bool connectedToNetwork();
std::string serializeTelemetryJson(uint64_t rx_bytes, uint64_t tx_bytes);
bool parseTelemetryJson(std::string_view resp, uint64_t *rx_bytes, uint64_t *tx_bytes);

constexpr char kAppMessageGetTelemetry[] = "__get_telemetry";

#endif //  YASS_IOS_UTILS
