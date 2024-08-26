// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_NETWORK
#define H_CONFIG_CONFIG_NETWORK

#include <absl/flags/declare.h>
#include <string>

ABSL_DECLARE_FLAG(bool, ipv6_mode);

ABSL_DECLARE_FLAG(bool, reuse_port);
ABSL_DECLARE_FLAG(bool, tcp_fastopen);
ABSL_DECLARE_FLAG(bool, tcp_fastopen_connect);
// same with proxy_connect_timeout no need for proxy_read_timeout
// and proxy_write_timeout because it is a tcp tunnel.
// TODO rename connect_timeout to proxy_connect_timeout
ABSL_DECLARE_FLAG(int32_t, connect_timeout);
ABSL_DECLARE_FLAG(bool, tcp_nodelay);

ABSL_DECLARE_FLAG(bool, tcp_keep_alive);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_cnt);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_idle_timeout);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_interval);
ABSL_DECLARE_FLAG(std::string, tcp_congestion_algorithm);
ABSL_DECLARE_FLAG(bool, redir_mode);

ABSL_DECLARE_FLAG(std::string, doh_url);
ABSL_DECLARE_FLAG(std::string, dot_host);
#endif  // H_CONFIG_CONFIG_NETWORK
