// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */

#include "cli/cli_connection_stats.hpp"

namespace net::cli {

std::atomic<uint64_t> total_rx_bytes;

std::atomic<uint64_t> total_tx_bytes;

}  // namespace net::cli
