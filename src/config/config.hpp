// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#include <absl/flags/declare.h>
#include <cstdint>
#include <string>

#include "crypto/crypter_export.hpp"

#include "config/config_network.hpp"
#include "config/config_tls.hpp"

struct PortFlag {
  explicit PortFlag(uint16_t p) : port(p) {}
  operator uint16_t() const { return port; }
  uint16_t port;
};

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m) : method(m) {}
  cipher_method method;
};

ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(std::string, server_sni);
ABSL_DECLARE_FLAG(PortFlag, server_port);
ABSL_DECLARE_FLAG(std::string, username);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(CipherMethodFlag, method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(PortFlag, local_port);

ABSL_DECLARE_FLAG(uint32_t, parallel_max);

struct RateFlag {
  explicit RateFlag(uint64_t r) : rate(r) {}
  uint64_t rate;
};
ABSL_DECLARE_FLAG(RateFlag, limit_rate);  // bytes per second

namespace config {
bool ReadConfig();
bool SaveConfig();

void ReadConfigFileAndArguments(int argc, const char** argv);

std::string ReadConfigFromArgument(const std::string& server_host,
                                   const std::string& server_sni,
                                   const std::string& server_port,
                                   const std::string& username,
                                   const std::string& password,
                                   cipher_method method,
                                   const std::string& local_host,
                                   const std::string& local_port,
                                   const std::string& doh_url,
                                   const std::string& dot_host,
                                   const std::string& connect_timeout);

std::string ReadConfigFromArgument(const std::string& server_host,
                                   const std::string& server_sni,
                                   const std::string& server_port,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& method_string,
                                   const std::string& local_host,
                                   const std::string& local_port,
                                   const std::string& doh_url,
                                   const std::string& dot_host,
                                   const std::string& connect_timeout);

void SetClientUsageMessage(const std::string& exec_path);
void SetServerUsageMessage(const std::string& exec_path);

}  // namespace config

#endif  // H_CONFIG_CONFIG
