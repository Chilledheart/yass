// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2023 Chilledheart  */

#ifndef H_CLI_CONNECTION_STATS
#define H_CLI_CONNECTION_STATS

#include <atomic>
#include <cstdint>

namespace cli {

/// statistics of total read bytes (non-encoded)
extern std::atomic<uint64_t> total_rx_bytes;
/// statistics of total sent bytes (non-encoded)
extern std::atomic<uint64_t> total_tx_bytes;

} // namespace cli

#endif  // H_CLI_CONNECTION_STATS
