// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "config/config_impl.hpp"

#include <absl/flags/flag.h>

#include "core/cipher.hpp"

#define MAX_CONNECT_TIMEOUT 10

ABSL_FLAG(std::string,
          server_host,
          "0.0.0.0",
          "IP address which remote server listens to");
ABSL_FLAG(int32_t,
          server_port,
          8443,
          "Port number which remote server listens to");
ABSL_FLAG(std::string, password, "<default-pass>", "Password pharsal");
ABSL_FLAG(std::string,
          method,
          CRYPTO_AES256GCMSHA256_STR,
          "Method of encrypt (internal)");
ABSL_FLAG(int32_t, cipher_method, CRYPTO_AES256GCMSHA256, "Method of encrypt");
ABSL_FLAG(std::string,
          local_host,
          "127.0.0.1",
          "IP address which local server listens to");
ABSL_FLAG(int32_t,
          local_port,
          8000,
          "Port number which local server listens to");
ABSL_FLAG(bool, reuse_port, true, "Reuse the listening port");
ABSL_FLAG(std::string, congestion_algorithm, "bbr", "TCP Congestion Algorithm");
ABSL_FLAG(bool, tcp_fastopen, false, "TCP fastopen");
ABSL_FLAG(bool, tcp_fastopen_connect, false, "TCP fastopen connect");
ABSL_FLAG(bool, auto_start, true, "Auto Start");

ABSL_FLAG(int32_t, connect_timeout, 60, "Connect timeout (Linux only)");
ABSL_FLAG(int32_t, tcp_user_timeout, 300, "TCP user timeout (Linux only)");
ABSL_FLAG(int32_t, so_linger_timeout, 30, "SO Linger timeout");

ABSL_FLAG(int32_t, so_snd_buffer, 16 * 1024, "Socket Send Buffer");
ABSL_FLAG(int32_t, so_rcv_buffer, 128 * 1024, "Socket Receive Buffer");

namespace config {

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
  config_impl->Read("auto_start", &FLAGS_auto_start);
  config_impl->Read("congestion_algorithm", &FLAGS_congestion_algorithm);
  config_impl->Read("timeout", &FLAGS_connect_timeout);
  config_impl->Read("connect_timeout", &FLAGS_connect_timeout);
  config_impl->Read("tcp_user_timeout", &FLAGS_tcp_user_timeout);
  config_impl->Read("so_linger_timeout", &FLAGS_so_linger_timeout);
  config_impl->Read("so_snd_buffer", &FLAGS_so_snd_buffer);
  config_impl->Read("so_rcv_buffer", &FLAGS_so_rcv_buffer);

  /* close fields */
  config_impl->Close();

  /* correct options */
  absl::SetFlag(
      &FLAGS_connect_timeout,
      std::max(MAX_CONNECT_TIMEOUT, absl::GetFlag(FLAGS_connect_timeout)));
  absl::SetFlag(&FLAGS_tcp_user_timeout,
                std::max(0, absl::GetFlag(FLAGS_tcp_user_timeout)));
  absl::SetFlag(&FLAGS_so_linger_timeout,
                std::max(0, absl::GetFlag(FLAGS_so_linger_timeout)));
  absl::SetFlag(&FLAGS_so_snd_buffer,
                std::max(0, absl::GetFlag(FLAGS_so_snd_buffer)));
  absl::SetFlag(&FLAGS_so_rcv_buffer,
                std::max(0, absl::GetFlag(FLAGS_so_rcv_buffer)));

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
  all_fields_written &= config_impl->Write("password", FLAGS_password);
  all_fields_written &= config_impl->Write("local", FLAGS_local_host);
  all_fields_written &= config_impl->Write("local_port", FLAGS_local_port);
  all_fields_written &=
      config_impl->Write("congestion_algorithm", FLAGS_congestion_algorithm);
  all_fields_written &= config_impl->Write("fast_open", FLAGS_tcp_fastopen);
  all_fields_written &=
      config_impl->Write("fast_open_connect", FLAGS_tcp_fastopen_connect);
  all_fields_written &= config_impl->Write("auto_start", FLAGS_auto_start);
  all_fields_written &= config_impl->Write("timeout", FLAGS_connect_timeout);
  all_fields_written &=
      config_impl->Write("connect_timeout", FLAGS_connect_timeout);
  all_fields_written &=
      config_impl->Write("tcp_user_timeout", FLAGS_tcp_user_timeout);
  all_fields_written &=
      config_impl->Write("so_linger_timeout", FLAGS_so_linger_timeout);
  all_fields_written &=
      config_impl->Write("so_snd_buffer", FLAGS_so_snd_buffer);
  all_fields_written &=
      config_impl->Write("so_rcv_buffer", FLAGS_so_rcv_buffer);

  all_fields_written &= config_impl->Close();

  return all_fields_written;
}

}  // namespace config
