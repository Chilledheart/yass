// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG
#define H_CONFIG_CONFIG

#include <string>
#include <string_view>

#include "config/config_core.hpp"
#include "config/config_network.hpp"
#include "config/config_tls.hpp"

#include "gui_variant.h"

namespace config {

bool ReadConfig();
bool SaveConfig();

enum ProgramType {
  YASS_UNSPEC,
  YASS_SERVER_MASK = 0x10,
  YASS_SERVER_DEFAULT = 0x10,
  YASS_UNITTEST_MASK = 0x20,
  YASS_UNITTEST_DEFAULT = 0x20,
  YASS_BENCHMARK_MASK = 0x40,
  YASS_BENCHMARK_DEFAULT = 0x40,
  YASS_CLIENT_MASK = 0x80,
  YASS_CLIENT_DEFAULT = 0x80,
  YASS_CLIENT_GUI = 0x81,
};

extern const ProgramType pType;
const char* ProgramTypeToStr(ProgramType type);

void ReadConfigFileAndArguments(int argc, const char** argv);

std::string ReadConfigFromArgument(std::string_view server_host,
                                   std::string_view server_sni,
                                   std::string_view server_port,
                                   std::string_view username,
                                   std::string_view password,
                                   cipher_method method,
                                   std::string_view local_host,
                                   std::string_view local_port,
                                   std::string_view doh_url,
                                   std::string_view dot_host,
                                   std::string_view limit_rate,
                                   std::string_view connect_timeout);

std::string ReadConfigFromArgument(std::string_view server_host,
                                   std::string_view server_sni,
                                   std::string_view server_port,
                                   std::string_view username,
                                   std::string_view password,
                                   std::string_view method_string,
                                   std::string_view local_host,
                                   std::string_view local_port,
                                   std::string_view doh_url,
                                   std::string_view dot_host,
                                   std::string_view limit_rate,
                                   std::string_view connect_timeout);

void SetClientUsageMessage(std::string_view exec_path);
void SetServerUsageMessage(std::string_view exec_path);

}  // namespace config

#endif  // H_CONFIG_CONFIG
