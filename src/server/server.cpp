// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "ss_factory.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "core/asio.hpp"
#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"

#ifndef SSMAXCONN
#define SSMAXCONN 1024
#endif

using namespace ss;

namespace {
asio::ip::tcp::endpoint resolveEndpoint(asio::io_context* io_context,
                                        const std::string& host,
                                        uint16_t port) {
  asio::error_code ec = asio::error_code();
  asio::ip::tcp::resolver resolver(*io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  if (ec) {
    LOG(WARNING) << "name resolved failed due to: " << ec;
    return asio::ip::tcp::endpoint();
  }
  return endpoints->endpoint();
}
}  // namespace

int main(int argc, const char* argv[]) {
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
  asio::executor_work_guard<asio::io_context::executor_type> work_guard(
      asio::make_work_guard(io_context));

  asio::ip::tcp::endpoint endpoint(
      resolveEndpoint(&io_context, absl::GetFlag(FLAGS_server_host),
                      absl::GetFlag(FLAGS_server_port)));
  // not used
  asio::ip::tcp::endpoint remoteEndpoint(
      resolveEndpoint(&io_context, absl::GetFlag(FLAGS_local_host),
                      absl::GetFlag(FLAGS_local_port)));

  LOG(WARNING) << "tcp server listening at " << endpoint;

  SsFactory factory(remoteEndpoint);
  asio::error_code ec;
  factory.listen(endpoint, SSMAXCONN, ec);
  if (ec) {
    LOG(ERROR) << "listen failed due to: " << ec;
    factory.stop();
    factory.join();
    work_guard.reset();
    return -1;
  }

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
  signals.async_wait([&](asio::error_code /*error*/, int /*signal_number*/) {
    factory.stop();
    factory.join();
    work_guard.reset();
  });

  io_context.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
    return -1;
  }

  return 0;
}
