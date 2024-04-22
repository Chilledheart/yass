// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_CORE
#define H_CONFIG_CONFIG_CORE

#include <absl/flags/declare.h>
#include <stdint.h>
#include <string>
#include <string_view>

#include "crypto/crypter_export.hpp"

struct PortFlag {
  explicit PortFlag(uint16_t p) : port(p) {}
  operator uint16_t() const { return port; }
  uint16_t port;
};

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m) : method(m) {}
  cipher_method method;
};

struct RateFlag {
  explicit RateFlag(uint64_t r) : rate(r) {}
  uint64_t rate;
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
ABSL_DECLARE_FLAG(RateFlag, limit_rate);  // bytes per second

#endif  // H_CONFIG_CONFIG_CORE
