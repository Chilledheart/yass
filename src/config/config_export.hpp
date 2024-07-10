// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef H_CONFIG_CONFIG_EXPORT
#define H_CONFIG_CONFIG_EXPORT

#include <absl/strings/str_cat.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include "crypto/crypter_export.hpp"

struct PortFlag {
  explicit PortFlag(uint16_t p = 0) : port(p) {}
  operator uint16_t() const { return port; }
  uint16_t port;
};

struct CipherMethodFlag {
  explicit CipherMethodFlag(cipher_method m = CRYPTO_INVALID) : method(m) {}
  operator std::string_view() const { return to_cipher_method_str(method); }
  operator cipher_method() const { return method; }
  cipher_method method;
};

struct RateFlag {
  explicit RateFlag(uint64_t r = 0u) : rate(r) {}
  operator std::string() const {
    if (rate % (1L << 20) == 0) {
      return absl::StrCat(rate / (1L << 20), "m");
    } else if (rate % (1L << 10) == 0) {
      return absl::StrCat(rate / (1L << 10), "k");
    }
    return absl::StrCat(rate);
  }
  operator uint64_t() const { return rate; }
  uint64_t rate;
};

#endif  // H_CONFIG_CONFIG_EXPORT
