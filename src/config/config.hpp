// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#include <cstdint>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <absl/flags/declare.h>
#include <string>

#include "network.hpp"
#include "core/cipher.hpp"

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m) : method(m) {}
  cipher_method method;
};

ABSL_DECLARE_FLAG(bool, ipv6_mode);
ABSL_DECLARE_FLAG(bool, io_queue_allow_merge);

ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(int32_t, server_port);
ABSL_DECLARE_FLAG(std::string, username);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(CipherMethodFlag, method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(int32_t, local_port);

ABSL_DECLARE_FLAG(std::string, certificate_chain_file);
ABSL_DECLARE_FLAG(std::string, private_key_file);
ABSL_DECLARE_FLAG(std::string, private_key_password);
ABSL_DECLARE_FLAG(bool, insecure_mode);
ABSL_DECLARE_FLAG(std::string, cacert);
ABSL_DECLARE_FLAG(std::string, capath);

ABSL_DECLARE_FLAG(uint32_t, worker_connections);

struct RateFlag {
  uint64_t rate;
};
ABSL_DECLARE_FLAG(RateFlag, limit_rate); //bytes per second

namespace config {
void ReadConfigFileOption(int argc, const char** argv);
bool ReadConfig();
bool SaveConfig();
}  // namespace config

#endif  // H_CONFIG_CONFIG
