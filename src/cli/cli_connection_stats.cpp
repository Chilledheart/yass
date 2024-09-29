// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */

#include "cli/cli_connection_stats.hpp"

#include <sstream>

#include "core/logging.hpp"
#include "core/utils.hpp"

namespace net::cli {

std::atomic<uint64_t> total_rx_bytes;
std::atomic<uint64_t> total_tx_bytes;

std::atomic<uint64_t> total_rx_times;
std::atomic<uint64_t> total_tx_times;

std::atomic<uint64_t> total_rx_yields;
std::atomic<uint64_t> total_tx_yields;

}  // namespace net::cli
static std::string HumanReadableByteCountBinStr(uint64_t bytes) {
  std::stringstream ss;

  HumanReadableByteCountBin(&ss, bytes);
  return ss.str();
}

void PrintCliStats() {
  LOG(ERROR) << "Cli Connection Stats: Sent: " << HumanReadableByteCountBinStr(net::cli::total_rx_bytes);
  LOG(ERROR) << "Cli Connection Stats: Received: " << HumanReadableByteCountBinStr(net::cli::total_tx_bytes);
  LOG(ERROR) << "Cli Connection Stats: Sent Times: " << net::cli::total_rx_times;
  LOG(ERROR) << "Cli Connection Stats: Received Times: " << net::cli::total_tx_times;
  LOG(ERROR) << "Cli Connection Stats: Sent Average: "
             << HumanReadableByteCountBinStr(net::cli::total_rx_bytes / (net::cli::total_rx_times + 1));
  LOG(ERROR) << "Cli Connection Stats: Received Average: "
             << HumanReadableByteCountBinStr(net::cli::total_tx_bytes / (net::cli::total_tx_times + 1));
  LOG(ERROR) << "Cli Connection Stats: Sent Yield Times: " << net::cli::total_rx_yields;
  LOG(ERROR) << "Cli Connection Stats: Received Yield Times: " << net::cli::total_tx_yields;
}
