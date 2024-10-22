// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "config/config_core.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <stdint.h>
#include <optional>
#include <sstream>

#include "core/utils.hpp"

// Within the implementation, `AbslParseFlag()` will, in turn invoke
// `absl::ParseFlag()` on its constituent `int` and `std::string` types
// (which have built-in Abseil flag support.

bool AbslParseFlag(absl::string_view text, PortFlag* flag, std::string* err) {
  int result;
  if (!StringToInt(text, &result) || result < 0 || result > UINT16_MAX) {
    *err = absl::StrCat("bad port number: ", text);
    return false;
  }
  flag->port = static_cast<uint16_t>(result);
  return true;
}

// Similarly, for unparsing, we can simply invoke `absl::UnparseFlag()` on
// the constituent types.
std::string AbslUnparseFlag(const PortFlag& flag) {
  return std::to_string(flag);
}

bool AbslParseFlag(absl::string_view text, CipherMethodFlag* flag, std::string* err) {
  flag->method = to_cipher_method(std::string(text));
  if (flag->method == CRYPTO_INVALID) {
    *err = absl::StrCat("bad cipher_method: ", text);
    return false;
  }
  return true;
}

// Similarly, for unparsing, we can simply invoke `absl::UnparseFlag()` on
// the constituent types.
std::string AbslUnparseFlag(const CipherMethodFlag& flag) {
  return std::string(flag);
}

// Within the implementation, `AbslParseFlag()` will, in turn invoke
// `absl::ParseFlag()` on its constituent `int` and `std::string` types
// (which have built-in Abseil flag support.

static int64_t ngx_atosz(const char* line, size_t n) {
  int64_t value, cutoff, cutlim;

  if (n == 0) {
    return -1;
  }

  cutoff = INT64_MAX / 10;
  cutlim = INT64_MAX % 10;

  for (value = 0; n--; line++) {
    if (*line < '0' || *line > '9') {
      return -1;
    }

    if (value >= cutoff && (value > cutoff || *line - '0' > cutlim)) {
      return -1;
    }

    value = value * 10 + (*line - '0');
  }

  return value;
}

static int64_t ngx_parse_size(const char* line, size_t len) {
  char unit;
  int64_t size, scale, max;

  if (len == 0) {
    return -1;
  }

  unit = line[len - 1];

  switch (unit) {
    case 'K':
    case 'k':
      len--;
      max = INT64_MAX / 1024;
      scale = 1024;
      break;

    case 'M':
    case 'm':
      len--;
      max = INT64_MAX / (1024 * 1024);
      scale = 1024 * 1024;
      break;

    default:
      max = INT64_MAX;
      scale = 1;
  }

  size = ngx_atosz(line, len);
  if (size < 0 || size > max) {
    return -1;
  }

  size *= scale;

  return size;
}

bool AbslParseFlag(absl::string_view text, RateFlag* flag, std::string* err) {
  if (text.empty()) {
    flag->rate = 0u;
    return true;
  }
  int64_t size = ngx_parse_size(text.data(), text.size());
  if (size < 0) {
    *err = absl::StrCat("bad size: ", text);
    return false;
  }
  flag->rate = size;
  return true;
}

// Similarly, for unparsing, we can simply invoke `absl::UnparseFlag()` on
// the constituent types.
std::string AbslUnparseFlag(const RateFlag& flag) {
  return flag;
}

ABSL_FLAG(std::string, server_host, "http2.github.io", "Remote server on given host");
ABSL_FLAG(std::string, server_sni, "", "Remote server on given sni");
ABSL_FLAG(PortFlag, server_port, PortFlag(443), "Remote server on given port");
ABSL_FLAG(std::string, local_host, "127.0.0.1", "Local proxy server on given host (Client Only)");
ABSL_FLAG(PortFlag, local_port, PortFlag(1080), "Local proxy server on given port (Client Only)");
ABSL_FLAG(std::string, username, "username", "Server user");
ABSL_FLAG(std::string, password, "password", "Server password");
static const std::string kCipherMethodHelpMessage =
    absl::StrCat("Specify encrypt of method to use, one of ", kCipherMethodsStr);
ABSL_FLAG(CipherMethodFlag, method, CipherMethodFlag(CRYPTO_DEFAULT), kCipherMethodHelpMessage);

ABSL_FLAG(uint32_t, parallel_max, 512, "Maximum concurrency for parallel connections");
ABSL_FLAG(RateFlag, limit_rate, RateFlag(0), "Limit transfer speed to RATE");

#if BUILDFLAG(IS_MAC)
ABSL_FLAG(bool, ui_display_realtime_status, true, "Display Realtime Status in Status Bar (UI)");
#endif
