// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_EXPORT
#define H_CONFIG_CONFIG_EXPORT

#include <stdint.h>
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

#endif  // H_CONFIG_CONFIG_EXPORT
