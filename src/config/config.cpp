// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "config/config.hpp"
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>

#include "core/cipher.hpp"

ABSL_FLAG(bool, ipv6_mode, true, "Enable IPv6 support");

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
ABSL_FLAG(std::string,
          method,
          CRYPTO_INVALID_STR,
          "Method of encrypt, such as http2");
ABSL_FLAG(int32_t, cipher_method, CRYPTO_HTTP2_TLS, "Method of encrypt (Internal Usage)");

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
    }
    ++pos;
  }
  if (!g_configfile.empty()) {
    fprintf(stderr, "loaded option from file: %s\n", g_configfile.c_str());
    fflush(stderr);
  }
}

bool ReadConfig() {
  auto config_impl = config::ConfigImpl::Create();
  bool required_fields_loaded = false;
  std::string cipher_method_str;

  if (!config_impl->Open(false)) {
    return false;
  }

  /* load required fields */
  required_fields_loaded &= config_impl->Read("server", &FLAGS_server_host);
  required_fields_loaded &=
      config_impl->Read("server_port", &FLAGS_server_port);
  required_fields_loaded &=
      /* new version field: higher priority */
      (config_impl->Read("cipher_method", &FLAGS_method) ||
       /* old version field: lower priority */
       config_impl->Read("method", &FLAGS_method));
  required_fields_loaded &= config_impl->Read("username", &FLAGS_username);
  required_fields_loaded &= config_impl->Read("password", &FLAGS_password);
  required_fields_loaded &= config_impl->Read("local", &FLAGS_local_host);
  required_fields_loaded &= config_impl->Read("local_port", &FLAGS_local_port);

  cipher_method_str = absl::GetFlag(FLAGS_method);
  auto cipher_method = to_cipher_method(cipher_method_str);
  if (cipher_method == CRYPTO_INVALID) {
    LOG(WARNING) << "bad cipher_method: " << cipher_method_str;
    required_fields_loaded = false;
  } else {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
    VLOG(1) << "loaded option cipher_method: " << cipher_method_str;
  }

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

  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));
  auto cipher_method_str = to_cipher_method_str(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)));
  absl::SetFlag(&FLAGS_method, cipher_method_str);

  if (!config_impl->Open(true)) {
    return false;
  }

  all_fields_written &= config_impl->Write("server", FLAGS_server_host);
  all_fields_written &= config_impl->Write("server_port", FLAGS_server_port);
  all_fields_written &= config_impl->Write("method", FLAGS_method);
  all_fields_written &= config_impl->Write("cipher_method", FLAGS_method);
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
