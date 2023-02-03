// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "server/ss_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <locale.h>

#include "core/asio.hpp"
#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"

#ifndef SSMAXCONN
#define SSMAXCONN 1024
#endif

using namespace ss;

int main(int argc, const char* argv[]) {
#ifdef OS_WIN
  if (!EnableSecureDllLoading()) {
    return -1;
  }
#endif
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::ParseCommandLine(argc, const_cast<char**>(argv));

  auto cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
  }

  config::ReadConfig();
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;

  // Start Io Context
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::io_context::work>(io_context);

  asio::ip::tcp::endpoint endpoint;
  std::string host_name = absl::GetFlag(FLAGS_server_host);
  uint16_t port = absl::GetFlag(FLAGS_server_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    endpoint = asio::ip::tcp::endpoint(addr, port);
  } else {
    asio::ip::tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(host_name, std::to_string(port), ec);
    if (ec) {
      LOG(WARNING) << "server resolved host:" << host_name
        << " failed due to: " << ec;
      return -1;
    }
    endpoint = endpoints->endpoint();
  }

  LOG(WARNING) << "tcp server listening at " << endpoint;

  SsServer server(io_context);
  server.listen(endpoint, SSMAXCONN, ec);
  if (ec) {
    LOG(ERROR) << "listen failed due to: " << ec;
    server.stop();
    work_guard.reset();
    return -1;
  }
  endpoint = server.endpoint();

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
  signals.async_wait([&](asio::error_code /*error*/, int /*signal_number*/) {
    server.stop();
    work_guard.reset();
  });

  io_context.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
    return -1;
  }

  return 0;
}
