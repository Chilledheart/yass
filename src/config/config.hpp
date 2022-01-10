// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#include <cstdint>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <absl/flags/declare.h>
#include <string>

// TBD: read settinsg from json config file

#define DEFAULT_SERVER "0.0.0.0"
#define DEFAULT_SERVER_PORT 8443
#define DEFAULT_PASS "<default-pass>"
#define DEFAULT_CIPHER CRYPTO_AES256GCMSHA256_STR
#define DEFAULT_LOCAL "127.0.0.1"
#define DEFAULT_LOCAL_PORT 8000
#define DEFAULT_REUSE_PORT true
#define DEFAULT_CONGESTION_ALGORITHM "bbr"
#define DEFAULT_TCP_FASTOPEN false
#define DEFAULT_AUTO_START true

#define DEFAULT_CONNECT_TIMEOUT 60
#define MAX_CONNECT_TIMEOUT 10

#define DEFAULT_TCP_USER_TIMEOUT 300
#define DEFAULT_SO_LINGER_TIMEOUT 30

#define DEFAULT_SO_SND_BUFFER (16 * 1024)
#define DEFAULT_SO_RCV_BUFFER (128 * 1024)

ABSL_DECLARE_FLAG(std::string, configfile);
ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(int32_t, server_port);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(std::string, method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(int32_t, local_port);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(bool,reuse_port);
ABSL_DECLARE_FLAG(std::string, congestion_algorithm);
ABSL_DECLARE_FLAG(bool,tcp_fastopen);
ABSL_DECLARE_FLAG(bool,tcp_fastopen_connect);
ABSL_DECLARE_FLAG(bool,auto_start);

ABSL_DECLARE_FLAG(int32_t, timeout);
ABSL_DECLARE_FLAG(int32_t, tcp_user_timeout);
ABSL_DECLARE_FLAG(int32_t, so_linger_timeout);

ABSL_DECLARE_FLAG(int32_t, so_snd_buffer);
ABSL_DECLARE_FLAG(int32_t, so_rcv_buffer);

namespace config {
bool ReadConfig();
bool SaveConfig();
} // namespace config

#endif // H_CONFIG_CONFIG
