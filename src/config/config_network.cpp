// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "config/config.hpp"

#include <absl/flags/flag.h>

ABSL_FLAG(bool, reuse_port, true, "Reuse the listening port");
ABSL_FLAG(std::string, congestion_algorithm, "bbr", "TCP Congestion Algorithm");
ABSL_FLAG(bool, tcp_fastopen, false, "TCP fastopen");
ABSL_FLAG(bool, tcp_fastopen_connect, false, "TCP fastopen connect");
ABSL_FLAG(int32_t, connect_timeout, 0, "Connect timeout (in seconds)");

ABSL_FLAG(bool, tcp_nodelay, true, "TCP_NODELAY option");

ABSL_FLAG(bool, tcp_keep_alive, true, "TCP keep alive option");
ABSL_FLAG(int32_t, tcp_keep_alive_cnt, 9, "The number of TCP keep-alive probes to send before give up.");
ABSL_FLAG(int32_t, tcp_keep_alive_idle_timeout, 7200, "The number of seconds a connection needs to be idle before TCP begins sending out keep-alive probes.");
ABSL_FLAG(int32_t, tcp_keep_alive_interval, 75, "The number of seconds between TCP keep-alive probes.");
ABSL_FLAG(bool, tls13_early_data, true, "Enable 0RTTI Early Data (risk at production)");
ABSL_FLAG(bool, redir_mode,
#ifdef __ANDROID__
          false,
#else
          true,
#endif
          "Enable TCP Redir mode support (linux/macOS only)");