// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */

#ifndef H_CLI_CONNECTION_STATS
#define H_CLI_CONNECTION_STATS

#include <atomic>
#include <cstdint>

namespace net::cli {

/// statistics of total received bytes (non-encoded)
extern std::atomic<uint64_t> total_rx_bytes;
/// statistics of total sent bytes (non-encoded)
extern std::atomic<uint64_t> total_tx_bytes;
/// statistics of total received times (non-encoded)
extern std::atomic<uint64_t> total_rx_times;
/// statistics of total sent times (non-encoded)
extern std::atomic<uint64_t> total_tx_times;
/// statistics of total yield times (rx) (non-encoded)
extern std::atomic<uint64_t> total_rx_yields;
/// statistics of total yield times (tx) (non-encoded)
extern std::atomic<uint64_t> total_tx_yields;

}  // namespace net::cli

void PrintCliStats();

#endif  // H_CLI_CONNECTION_STATS
