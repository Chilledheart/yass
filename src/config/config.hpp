// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#include <cstdint>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <absl/flags/declare.h>
#include <string>

#include "crypto/crypter_export.hpp"

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m) : method(m) {}
  cipher_method method;
};

ABSL_DECLARE_FLAG(bool, ipv6_mode);

ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(std::string, server_sni);
ABSL_DECLARE_FLAG(int32_t, server_port);
ABSL_DECLARE_FLAG(std::string, username);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(CipherMethodFlag, method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(int32_t, local_port);

// config_tls.cpp
extern std::string g_certificate_chain_content;
extern std::string g_private_key_content;
ABSL_DECLARE_FLAG(std::string, certificate_chain_file);
ABSL_DECLARE_FLAG(std::string, private_key_file);
ABSL_DECLARE_FLAG(std::string, private_key_password);
ABSL_DECLARE_FLAG(bool, insecure_mode);
ABSL_DECLARE_FLAG(std::string, cacert);
ABSL_DECLARE_FLAG(std::string, capath);

ABSL_DECLARE_FLAG(uint32_t, worker_connections);

struct RateFlag {
  explicit RateFlag(uint64_t r) : rate(r) {}
  uint64_t rate;
};
ABSL_DECLARE_FLAG(RateFlag, limit_rate); //bytes per second

// config_network.cpp
ABSL_DECLARE_FLAG(bool, reuse_port);
ABSL_DECLARE_FLAG(std::string, congestion_algorithm);
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
ABSL_DECLARE_FLAG(bool, tls13_early_data);
ABSL_DECLARE_FLAG(bool, redir_mode);

namespace config {
void ReadConfigFileOption(int argc, const char** argv);
bool ReadConfig();
bool SaveConfig();

std::string ReadConfigFromArgument(const std::string& server_host,
                                   const std::string& server_sni,
                                   const std::string& server_port,
                                   const std::string& username,
                                   const std::string& password,
                                   cipher_method method,
                                   const std::string& local_host,
                                   const std::string& local_port,
                                   const std::string& connect_timeout);

std::string ReadConfigFromArgument(const std::string& server_host,
                                   const std::string& server_sni,
                                   const std::string& server_port,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& method_string,
                                   const std::string& local_host,
                                   const std::string& local_port,
                                   const std::string& connect_timeout);
}  // namespace config

#endif  // H_CONFIG_CONFIG
