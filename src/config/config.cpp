//
// config.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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
DEFINE_bool(reuse_port, true, "Reuse the local port");

namespace config {

bool ReadConfig() {
  auto config_impl = config::ConfigImpl::Create();

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
      !config_impl->Close()) {
    return false;
  }

  VLOG(1) << "saved option server: " << FLAGS_server_host;
  VLOG(1) << "saved option server_port: " << FLAGS_server_port;
  VLOG(1) << "saved option method: " << FLAGS_method;
  VLOG(1) << "saved option password: " << FLAGS_password;
  VLOG(1) << "saved option local: " << FLAGS_local_host;
  VLOG(1) << "saved option local_port: " << FLAGS_local_port;

  return true;
}

} // namespace config
