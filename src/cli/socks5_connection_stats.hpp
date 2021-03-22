// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#ifndef H_SOCKS5_CONNECTION_STATS
#define H_SOCKS5_CONNECTION_STATS

#include <atomic>

/// statistics of total read bytes (non-encoded)
extern std::atomic_uint64_t total_rx_bytes;
/// statistics of total sent bytes (non-encoded)
extern std::atomic_uint64_t total_tx_bytes;

#endif // H_SOCKS5_CONNECTION_STATS
