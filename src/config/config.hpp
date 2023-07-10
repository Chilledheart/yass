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

ABSL_DECLARE_FLAG(std::string, server_host);
ABSL_DECLARE_FLAG(int32_t, server_port);
ABSL_DECLARE_FLAG(std::string, username);
ABSL_DECLARE_FLAG(std::string, password);
ABSL_DECLARE_FLAG(std::string, method);
ABSL_DECLARE_FLAG(int32_t, cipher_method);
ABSL_DECLARE_FLAG(std::string, local_host);
ABSL_DECLARE_FLAG(int32_t, local_port);

ABSL_DECLARE_FLAG(std::string, certificate_chain_file);
ABSL_DECLARE_FLAG(std::string, private_key_file);
ABSL_DECLARE_FLAG(std::string, private_key_password);
ABSL_DECLARE_FLAG(bool, insecure_mode);
ABSL_DECLARE_FLAG(std::string, cacert);
ABSL_DECLARE_FLAG(std::string, capath);

ABSL_DECLARE_FLAG(bool, ipv6_mode);

namespace config {
bool ReadConfig();
bool SaveConfig();
}  // namespace config

#endif  // H_CONFIG_CONFIG
