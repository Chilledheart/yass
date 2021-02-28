// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "config/config_impl.hpp"

#include <gflags/gflags.h>

#include "core/cipher.hpp"

DEFINE_string(server_host, DEFAULT_SERVER,
              "IP address which remote server listens to");
DEFINE_int32(server_port, DEFAULT_SERVER_PORT,
             "Port number which remote server listens to");
DEFINE_string(password, DEFAULT_PASS, "Password pharsal");
DEFINE_string(method, DEFAULT_CIPHER, "Method of encrypt");
DEFINE_string(local_host, DEFAULT_LOCAL,
              "IP address which local server listens to");
DEFINE_int32(local_port, DEFAULT_LOCAL_PORT,
             "Port number which local server listens to");
DEFINE_bool(reuse_port, DEFAULT_REUSE_PORT, "Reuse the listening port");
DEFINE_bool(tcp_fastopen, DEFAULT_TCP_FASTOPEN, "TCP fastopen");
DEFINE_bool(tcp_fastopen_connect, DEFAULT_TCP_FASTOPEN, "TCP fastopen connect");
DEFINE_bool(auto_start, DEFAULT_AUTO_START, "Auto Start");
DEFINE_string(congestion_algorithm, DEFAULT_CONGESTION_ALGORITHM, "TCP Congestion Algorithm");

namespace config {

bool ReadConfig() {
  auto config_impl = config::ConfigImpl::Create();

  /* required fields */
  if (!config_impl->Open(false) ||
      !config_impl->Read("server", &FLAGS_server_host) ||
      !config_impl->Read("server_port", &FLAGS_server_port) ||
      !config_impl->Read("method", &FLAGS_method) ||
      !config_impl->Read("password", &FLAGS_password) ||
      !config_impl->Read("local", &FLAGS_local_host) ||
      !config_impl->Read("local_port", &FLAGS_local_port) ||
      !config_impl->Close()) {
    return false;
  }

  cipher_method_in_use = to_cipher_method(FLAGS_method);
  if (cipher_method_in_use == CRYPTO_PLAINTEXT) {
    LOG(WARNING) << "bad method: " << FLAGS_method;
    return false;
  }

  VLOG(1) << "loaded option server: " << FLAGS_server_host;
  VLOG(1) << "loaded option server_port: " << FLAGS_server_port;
  VLOG(1) << "loaded option method: " << FLAGS_method;
  VLOG(1) << "loaded option password: " << FLAGS_password;
  VLOG(1) << "loaded option local: " << FLAGS_local_host;
  VLOG(1) << "loaded option local_port: " << FLAGS_local_port;

  /* optional fields */
  config_impl->Read("fast_open", &FLAGS_tcp_fastopen);
  config_impl->Read("fast_open", &FLAGS_tcp_fastopen_connect);
  config_impl->Read("auto_start", &FLAGS_auto_start);
  config_impl->Read("congestion_algorithm", &FLAGS_congestion_algorithm);
  VLOG(1) << "loaded option fast_open: " << std::boolalpha << FLAGS_tcp_fastopen;
  VLOG(1) << "loaded option auto_start: " << std::boolalpha << FLAGS_auto_start;
  VLOG(1) << "loaded option congestion_algorithm: " << FLAGS_congestion_algorithm;

  VLOG(1) << "initializing ciphers... " << FLAGS_method;

  if (FLAGS_reuse_port) {
    LOG(WARNING) << "using port reuse";
  }
#if defined(TCP_FASTOPEN)
  if (FLAGS_tcp_fastopen) {
    LOG(WARNING) << "using tcp fast open";
  }
#endif
#if defined(TCP_FASTOPEN_CONNECT)
  if (FLAGS_tcp_fastopen_connect) {
    LOG(WARNING) << "using tcp fast open connect";
  }
#endif
  if (FLAGS_auto_start) {
    LOG(WARNING) << "using autostart";
  }
#if defined(TCP_CONGESTION)
  LOG(WARNING) << "using congestion: " << FLAGS_congestion_algorithm;
#endif

  return true;
}

bool SaveConfig() {
  FLAGS_method = to_cipher_method_str(cipher_method_in_use);

  if (cipher_method_in_use == CRYPTO_PLAINTEXT) {
    return false;
  }

  auto config_impl = config::ConfigImpl::Create();

  if (!config_impl->Open(true) ||
      !config_impl->Write("server", FLAGS_server_host) ||
      !config_impl->Write("server_port", FLAGS_server_port) ||
      !config_impl->Write("method", FLAGS_method) ||
      !config_impl->Write("password", FLAGS_password) ||
      !config_impl->Write("local", FLAGS_local_host) ||
      !config_impl->Write("local_port", FLAGS_local_port) ||
      !config_impl->Write("auto_start", FLAGS_auto_start) ||
      !config_impl->Write("fast_open", FLAGS_tcp_fastopen) ||
      !config_impl->Write("congestion_algorithm", FLAGS_congestion_algorithm) ||
      !config_impl->Close()) {
    return false;
  }

  VLOG(1) << "saved option server: " << FLAGS_server_host;
  VLOG(1) << "saved option server_port: " << FLAGS_server_port;
  VLOG(1) << "saved option method: " << FLAGS_method;
  VLOG(1) << "saved option password: " << FLAGS_password;
  VLOG(1) << "saved option local: " << FLAGS_local_host;
  VLOG(1) << "saved option local_port: " << FLAGS_local_port;

  VLOG(1) << "saved option fast_open: " << std::boolalpha << FLAGS_tcp_fastopen;
  VLOG(1) << "saved option auto_start: " << std::boolalpha << FLAGS_auto_start;
  VLOG(1) << "saved option congestion_algorithm: " << FLAGS_congestion_algorithm;

  return true;
}

} // namespace config
