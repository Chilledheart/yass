// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "config/config.hpp"
#include "config/config_impl.hpp"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <absl/flags/internal/program_name.h>

#include "core/cipher.hpp"
#include "version.h"

bool AbslParseFlag(absl::string_view text, CipherMethodFlag* flag,
                   std::string* err);

std::string AbslUnparseFlag(const CipherMethodFlag&);

bool AbslParseFlag(absl::string_view text, RateFlag* flag,
                   std::string* err);

std::string AbslUnparseFlag(const RateFlag&);

// Within the implementation, `AbslParseFlag()` will, in turn invoke
// `absl::ParseFlag()` on its constituent `int` and `std::string` types
// (which have built-in Abseil flag support.

bool AbslParseFlag(absl::string_view text, CipherMethodFlag* flag,
                   std::string* err) {
  flag->method = to_cipher_method(std::string(text));
  if (flag->method == CRYPTO_INVALID) {
    *err = "bad cipher_method: " + std::string(text);
    return false;
  }
  return true;
}

// Similarly, for unparsing, we can simply invoke `absl::UnparseFlag()` on
// the constituent types.
std::string AbslUnparseFlag(const CipherMethodFlag& flag) {
  DCHECK(is_valid_cipher_method(flag.method));
  return to_cipher_method_str(flag.method);
}

// Within the implementation, `AbslParseFlag()` will, in turn invoke
// `absl::ParseFlag()` on its constituent `int` and `std::string` types
// (which have built-in Abseil flag support.

static int64_t ngx_atosz(const char *line, size_t n) {
  int64_t  value, cutoff, cutlim;

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

static int64_t ngx_parse_size(const char *line, size_t len) {
  char   unit;
  int64_t  size, scale, max;

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

bool AbslParseFlag(absl::string_view text, RateFlag* flag,
                   std::string* err) {
  int64_t size = ngx_parse_size(text.data(), text.size());
  if (size < 0) {
    *err = "bad size: " + std::string(text);
    return false;
  }
  flag->rate = size;
  return true;
}

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes;
    return;
  }
  if (bytes < 1024 * 1024) {
    *ss << bytes / 1024 << "k";
    return;
  }
  *ss << bytes / 1024 / 1024 << "m";
}

// Similarly, for unparsing, we can simply invoke `absl::UnparseFlag()` on
// the constituent types.
std::string AbslUnparseFlag(const RateFlag& flag) {
  std::ostringstream os;
  humanReadableByteCountBin(&os, flag.rate);
  return os.str();
}

ABSL_FLAG(bool, ipv6_mode, true, "Enable IPv6 support");
ABSL_FLAG(bool, io_queue_allow_merge, true, "Allow IoQueue push_back_merge");

ABSL_FLAG(std::string,
          server_host,
          "0.0.0.0",
          "Host address which remote server listens to");
ABSL_FLAG(int32_t,
          server_port,
          8443,
          "Port number which remote server listens to");
ABSL_FLAG(std::string,
          local_host,
          "127.0.0.1",
          "Host address which local server listens to");
ABSL_FLAG(int32_t,
          local_port,
          8000,
          "Port number which local server listens to");

ABSL_FLAG(std::string, username, "<default-user>", "Username");
ABSL_FLAG(std::string, password, "<default-pass>", "Password pharsal");
static const std::string kCipherMethodHelpMessage =
    absl::StrCat("Method of encrypt, one of ",
                 absl::string_view(kCipherMethodsStr, strlen(kCipherMethodsStr)-2));
ABSL_FLAG(CipherMethodFlag,
          method,
          CipherMethodFlag(CRYPTO_HTTP2),
          kCipherMethodHelpMessage);

ABSL_FLAG(uint32_t,
          worker_connections,
          512,
          "Maximum number of accepted connection");

ABSL_FLAG(RateFlag,
          limit_rate,
          RateFlag(),
          "Limits the rate of response transmission to a client. Uint is byte per second");

namespace config {

void ReadConfigFileOption(int argc, const char** argv) {
  int pos = 1;
  while (pos < argc) {
    std::string arg = argv[pos];
    if (pos + 1 < argc && (arg == "-v" || arg == "--v")) {
      absl::SetFlag(&FLAGS_v, atoi(argv[pos + 1]));
      argv[pos] = "";
      argv[pos+1] = "";
      pos += 2;
      continue;
    } else if (pos + 1 < argc && (arg == "-vmodule" || arg == "--vmodule")) {
      absl::SetFlag(&FLAGS_vmodule, argv[pos + 1]);
      argv[pos] = "";
      argv[pos+1] = "";
      pos += 2;
      continue;
    } else if (arg == "-logtostderr" || arg == "-logtostderr=true" ||
               arg == "--logtostderr" || arg == "--logtostderr=true") {
      absl::SetFlag(&FLAGS_logtostderr, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-nologtostderr" || arg == "-logtostderr=false" ||
               arg == "--nologtostderr" || arg == "--logtostderr=false") {
      absl::SetFlag(&FLAGS_logtostderr, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-alsologtostderr" || arg == "-alsologtostderr=true" ||
               arg == "--alsologtostderr" || arg == "--alsologtostderr=true") {
      absl::SetFlag(&FLAGS_alsologtostderr, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-noalsologtostderr" || arg == "-alsologtostderr=false" ||
               arg == "--noalsologtostderr" || arg == "--alsologtostderr=false") {
      absl::SetFlag(&FLAGS_alsologtostderr, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-colorlogtostderr" || arg == "-colorlogtostderr=true" ||
               arg == "--colorlogtostderr" || arg == "--colorlogtostderr=true") {
      absl::SetFlag(&FLAGS_colorlogtostderr, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-nocolorlogtostderr" || arg == "-colorlogtostderr=false" ||
               arg == "--nocolorlogtostderr" || arg == "--colorlogtostderr=false") {
      absl::SetFlag(&FLAGS_colorlogtostderr, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-k" || arg == "--k" ||
               arg == "-insecure_mode" || arg == "--insecure_mode") {
      absl::SetFlag(&FLAGS_insecure_mode, true);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (arg == "-noinsecure_mode" || arg == "-insecure_mode=false" ||
               arg == "--noinsecure_mode" || arg == "--insecure_mode=false") {
      absl::SetFlag(&FLAGS_insecure_mode, false);
      argv[pos] = "";
      pos += 1;
      continue;
    } else if (pos + 1 < argc && (arg == "-c" || arg == "--configfile")) {
      g_configfile = argv[pos + 1];
      argv[pos] = "";
      argv[pos+1] = "";
      pos += 2;
      continue;
    } else if (arg == "--version") {
      fprintf(stdout, "%s %s\n", absl::flags_internal::ShortProgramInvocationName().c_str(),
              YASS_APP_TAG);
      fprintf(stdout, "Last Change: %s\n", YASS_APP_LAST_CHANGE);
#ifndef NDEBUG
      fprintf(stdout, "Debug build (NDEBUG not #defined)\n");
#endif
      fflush(stdout);
      argv[pos] = "";
      pos += 1;
      exit(0);
    }
    ++pos;
  }

  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;
  LOG(WARNING) << "Last Change: " << YASS_APP_LAST_CHANGE;
#ifndef NDEBUG
  LOG(WARNING) << "Debug build (NDEBUG not #defined)\n";
#endif
}

bool ReadConfig() {
  auto config_impl = config::ConfigImpl::Create();
  bool required_fields_loaded = false;

  if (!config_impl->Open(false)) {
    return false;
  }

  /* load required fields */
  required_fields_loaded &= config_impl->Read("server", &FLAGS_server_host);
  required_fields_loaded &=
      config_impl->Read("server_port", &FLAGS_server_port);
  required_fields_loaded &= config_impl->Read("method", &FLAGS_method);
  required_fields_loaded &= config_impl->Read("username", &FLAGS_username);
  required_fields_loaded &= config_impl->Read("password", &FLAGS_password);
  required_fields_loaded &= config_impl->Read("local", &FLAGS_local_host);
  required_fields_loaded &= config_impl->Read("local_port", &FLAGS_local_port);

  /* optional fields */
  config_impl->Read("fast_open", &FLAGS_tcp_fastopen);
  config_impl->Read("fast_open_connect", &FLAGS_tcp_fastopen_connect);

  config_impl->Read("congestion_algorithm", &FLAGS_congestion_algorithm);
  config_impl->Read("connect_timeout", &FLAGS_connect_timeout);
  config_impl->Read("tcp_nodelay", &FLAGS_tcp_nodelay);

  config_impl->Read("tcp_keep_alive", &FLAGS_tcp_keep_alive);
  config_impl->Read("tcp_keep_alive_cnt", &FLAGS_tcp_keep_alive_cnt);
  config_impl->Read("tcp_keep_alive_idle_timeout", &FLAGS_tcp_keep_alive_idle_timeout);
  config_impl->Read("tcp_keep_alive_interval", &FLAGS_tcp_keep_alive_interval);

  /* close fields */
  config_impl->Close();

  /* correct options */
  absl::SetFlag(&FLAGS_connect_timeout,
                std::max(0, absl::GetFlag(FLAGS_connect_timeout)));

  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt,
                std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_cnt)));
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout,
                std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout)));
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval,
                std::max(0, absl::GetFlag(FLAGS_tcp_keep_alive_interval)));

  return required_fields_loaded;
}

bool SaveConfig() {
  auto config_impl = config::ConfigImpl::Create();
  bool all_fields_written = false;

  if (!config_impl->Open(true)) {
    return false;
  }

  all_fields_written &= config_impl->Write("server", FLAGS_server_host);
  all_fields_written &= config_impl->Write("server_port", FLAGS_server_port);
  all_fields_written &= config_impl->Write("method", FLAGS_method);
  all_fields_written &= config_impl->Write("username", FLAGS_username);
  all_fields_written &= config_impl->Write("password", FLAGS_password);
  all_fields_written &= config_impl->Write("local", FLAGS_local_host);
  all_fields_written &= config_impl->Write("local_port", FLAGS_local_port);

  all_fields_written &= config_impl->Write("fast_open", FLAGS_tcp_fastopen);
  all_fields_written &=
      config_impl->Write("fast_open_connect", FLAGS_tcp_fastopen_connect);
  static_cast<void>(config_impl->Delete("threads"));
  all_fields_written &=
      config_impl->Write("congestion_algorithm", FLAGS_congestion_algorithm);
  all_fields_written &= config_impl->Write("timeout", FLAGS_connect_timeout);
  all_fields_written &=
      config_impl->Write("connect_timeout", FLAGS_connect_timeout);
  all_fields_written &=
      config_impl->Write("tcp_nodelay", FLAGS_tcp_nodelay);

  all_fields_written &=
      config_impl->Write("tcp_keep_alive", FLAGS_tcp_keep_alive);
  all_fields_written &=
      config_impl->Write("tcp_keep_alive_cnt", FLAGS_tcp_keep_alive_cnt);
  all_fields_written &=
      config_impl->Write("tcp_keep_alive_idle_timeout", FLAGS_tcp_keep_alive_idle_timeout);
  all_fields_written &=
      config_impl->Write("tcp_keep_alive_interval", FLAGS_tcp_keep_alive_interval);

  all_fields_written &= config_impl->Close();

  return all_fields_written;
}

}  // namespace config
