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
ABSL_FLAG(std::string, method, CRYPTO_AES256GCMSHA256_STR, "Method of encrypt (internal)");
ABSL_FLAG(int32_t, cipher_method, CRYPTO_AES256GCMSHA256, "Method of encrypt");
ABSL_FLAG(std::string,
          local_host,
          "127.0.0.1",
          "IP address which local server listens to");
ABSL_FLAG(int32_t,
          local_port,
          8000,
          "Port number which local server listens to");
ABSL_FLAG(bool,reuse_port, true, "Reuse the listening port");
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
  bool required_fields_loaded;

  if (!config_impl->Open(false))
    return false;

  /* load required fields */
  std::string cipher_method_str;
  required_fields_loaded =
      config_impl->Read("server", &FLAGS_server_host) &&
      config_impl->Read("server_port", &FLAGS_server_port) &&
      /* new version field: higher priority */
      (config_impl->Read("cipher_method", &cipher_method_str) ||
       /* old version field: lower priority */
       config_impl->Read("method", &cipher_method_str)) &&
      config_impl->Read("password", &FLAGS_password) &&
      config_impl->Read("local", &FLAGS_local_host) &&
      config_impl->Read("local_port", &FLAGS_local_port);

  auto cipher_method = to_cipher_method(cipher_method_str);
  if (cipher_method == CRYPTO_INVALID) {
    LOG(WARNING) << "bad method: " << cipher_method_str;
    required_fields_loaded = false;
  } else {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
  }

  VLOG(1) << "loaded option server: " << absl::GetFlag(FLAGS_server_host);
  VLOG(1) << "loaded option server_port: " << absl::GetFlag(FLAGS_server_port);
  VLOG(1) << "loaded option cipher_method: " << cipher_method_str;
  VLOG(1) << "loaded option password: " << absl::GetFlag(FLAGS_password);
  VLOG(1) << "loaded option local: " << absl::GetFlag(FLAGS_local_host);
  VLOG(1) << "loaded option local_port: " << absl::GetFlag(FLAGS_local_port);

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

  VLOG(1) << "loaded option congestion_algorithm: "
          << absl::GetFlag(FLAGS_congestion_algorithm);
  VLOG(1) << "loaded option fast_open: " << std::boolalpha
    << absl::GetFlag(FLAGS_tcp_fastopen);
  VLOG(1) << "loaded option fast_open_connect: " << std::boolalpha
    << absl::GetFlag(FLAGS_tcp_fastopen_connect);
  VLOG(1) << "loaded option auto_start: " << std::boolalpha
    << absl::GetFlag(FLAGS_auto_start);
  VLOG(1) << "loaded option timeout: " << absl::GetFlag(FLAGS_connect_timeout);
  VLOG(1) << "loaded option tcp_user_timeout: "
    << absl::GetFlag(FLAGS_tcp_user_timeout);
  VLOG(1) << "loaded option so_linger_timeout: "
    << absl::GetFlag(FLAGS_so_linger_timeout);
  VLOG(1) << "loaded option so_snd_buffer: "
    << absl::GetFlag(FLAGS_so_snd_buffer);
  VLOG(1) << "loaded option so_rcv_buffer: "
    << absl::GetFlag(FLAGS_so_rcv_buffer);

  VLOG(1) << "initializing ciphers... " << cipher_method_str;

  if (absl::GetFlag(FLAGS_reuse_port)) {
    LOG(WARNING) << "using port reuse";
  }
#if defined(TCP_CONGESTION)
  LOG(WARNING) << "using congestion: "
    << absl::GetFlag(FLAGS_congestion_algorithm);
#endif
#if defined(TCP_FASTOPEN)
  if (absl::GetFlag(FLAGS_tcp_fastopen)) {
    LOG(WARNING) << "using tcp fast open";
  }
#endif
#if defined(TCP_FASTOPEN_CONNECT)
  if (absl::GetFlag(FLAGS_tcp_fastopen_connect)) {
    LOG(WARNING) << "using tcp fast open connect";
  }
#endif
  if (absl::GetFlag(FLAGS_auto_start)) {
    LOG(WARNING) << "using autostart";
  }

  /* close fields */
  config_impl->Close();

  return required_fields_loaded;
}

bool SaveConfig() {
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));
  auto cipher_method_str = to_cipher_method_str(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)));

  auto config_impl = config::ConfigImpl::Create();

  if (!config_impl->Open(true) ||
      !config_impl->Write("server", FLAGS_server_host) ||
      !config_impl->Write("server_port", FLAGS_server_port) ||
      !config_impl->Write("method", cipher_method_str) ||
      !config_impl->Write("cipher_method", cipher_method_str) ||
      !config_impl->Write("password", FLAGS_password) ||
      !config_impl->Write("local", FLAGS_local_host) ||
      !config_impl->Write("local_port", FLAGS_local_port) ||
      !config_impl->Write("congestion_algorithm", FLAGS_congestion_algorithm) ||
      !config_impl->Write("fast_open", FLAGS_tcp_fastopen) ||
      !config_impl->Write("fast_open_connect", FLAGS_tcp_fastopen_connect) ||
      !config_impl->Write("auto_start", FLAGS_auto_start) ||
      !config_impl->Write("timeout", FLAGS_connect_timeout) ||
      !config_impl->Write("connect_timeout", FLAGS_connect_timeout) ||
      !config_impl->Write("tcp_user_timeout", FLAGS_tcp_user_timeout) ||
      !config_impl->Write("so_linger_timeout", FLAGS_so_linger_timeout) ||
      !config_impl->Write("so_snd_buffer", FLAGS_so_snd_buffer) ||
      !config_impl->Write("so_rcv_buffer", FLAGS_so_rcv_buffer) ||
      !config_impl->Close()) {
    return false;
  }

  VLOG(1) << "saved option server: " << absl::GetFlag(FLAGS_server_host);
  VLOG(1) << "saved option server_port: " << absl::GetFlag(FLAGS_server_port);
  VLOG(1) << "saved option method: " << cipher_method_str;
  VLOG(1) << "saved option cipher_method: " << cipher_method_str;
  VLOG(1) << "saved option password: " << absl::GetFlag(FLAGS_password);
  VLOG(1) << "saved option local: " << absl::GetFlag(FLAGS_local_host);
  VLOG(1) << "saved option local_port: " << absl::GetFlag(FLAGS_local_port);

  VLOG(1) << "saved option congestion_algorithm: "
          << absl::GetFlag(FLAGS_congestion_algorithm);
  VLOG(1) << "saved option fast_open: " << std::boolalpha
          << absl::GetFlag(FLAGS_tcp_fastopen);
  VLOG(1) << "saved option fast_open_connect: " << std::boolalpha
          << absl::GetFlag(FLAGS_tcp_fastopen_connect);
  VLOG(1) << "saved option auto_start: " << std::boolalpha
          << absl::GetFlag(FLAGS_auto_start);
  VLOG(1) << "saved option connect timeout: "
          << absl::GetFlag(FLAGS_connect_timeout);
  VLOG(1) << "saved option tcp_user_timeout: "
          << absl::GetFlag(FLAGS_tcp_user_timeout);
  VLOG(1) << "saved option so_linger_timeout: "
          << absl::GetFlag(FLAGS_so_linger_timeout);
  VLOG(1) << "saved option so_snd_buffer: "
          << absl::GetFlag(FLAGS_so_snd_buffer);
  VLOG(1) << "saved option so_rcv_buffer: "
          << absl::GetFlag(FLAGS_so_rcv_buffer);

  return true;
}

} // namespace config
