// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#ifdef _WIN32
#include <malloc.h>
#endif

#include <gflags/gflags_declare.h>


// TBD: read settinsg from json config file

#define DEFAULT_SERVER "0.0.0.0"
#define DEFAULT_SERVER_PORT 8443
#define DEFAULT_PASS "<default-pass>"
#define DEFAULT_CIPHER CRYPTO_PLAINTEXT_STR
#define DEFAULT_LOCAL "127.0.0.1"
#define DEFAULT_LOCAL_PORT 8000
#define DEFAULT_REUSE_PORT true
#define DEFAULT_CONGESTION_ALGORITHM "bbr"
#define DEFAULT_TCP_FASTOPEN true
#define DEFAULT_AUTO_START true

#define DEFAULT_CONNECT_TIMEOUT 60
#define MAX_CONNECT_TIMEOUT 10

#define DEFAULT_TCP_USER_TIMEOUT 300

DECLARE_string(configfile);
DECLARE_string(server_host);
DECLARE_int32(server_port);
DECLARE_string(password);
DECLARE_string(method);
DECLARE_string(local_host);
DECLARE_int32(local_port);
DECLARE_string(password);
DECLARE_bool(reuse_port);
DECLARE_string(congestion_algorithm);
DECLARE_bool(tcp_fastopen);
DECLARE_bool(tcp_fastopen_connect);
DECLARE_bool(auto_start);

DECLARE_int32(timeout);
DECLARE_int32(tcp_user_timeout);

namespace config {
bool ReadConfig();
bool SaveConfig();
} // namespace config

#endif // H_CONFIG_CONFIG
