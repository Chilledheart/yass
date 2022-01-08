// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "ss_factory.hpp"

#include <gflags/gflags.h>
#include <asio.hpp>

#include "core/logging.hpp"

#ifndef SSMAXCONN
#define SSMAXCONN 1024
#endif

using namespace ss;

static asio::ip::tcp::endpoint resolveEndpoint(asio::io_context* io_context,
                                               const std::string& host,
                                               uint16_t port) {
  asio::error_code ec;
  asio::ip::tcp::resolver resolver(*io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  return endpoints->endpoint();
}

int main(int argc, const char* argv[]) {
  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  asio::io_context io_context;
  // Start Io Context
  ::google::InitGoogleLogging(argv[0]);
  ::FLAGS_stderrthreshold = true;
#ifndef NDEBUG
  ::FLAGS_logtostderr = true;
  ::FLAGS_logbuflevel = 0;
  ::FLAGS_v = 1;
#else
  ::FLAGS_logbuflevel = 1;
  ::FLAGS_v = 2;
#endif
  ::google::ParseCommandLineFlags(&argc, const_cast<char***>(&argv), true);
  ::google::InstallFailureSignalHandler();

  (void)config::ReadConfig();
  if (cipher_method_in_use == CRYPTO_INVALID) {
    return -1;
  }

  asio::ip::tcp::endpoint endpoint(
      resolveEndpoint(&io_context, FLAGS_server_host, FLAGS_server_port));
  // not used
  asio::ip::tcp::endpoint remoteEndpoint(
      resolveEndpoint(&io_context, FLAGS_local_host, FLAGS_local_port));

  LOG(WARNING) << "tcp server listening at " << endpoint;

  SsFactory factory(io_context, remoteEndpoint);
  asio::error_code ec;
  factory.listen(endpoint, SSMAXCONN, ec);
  if (ec) {
    LOG(ERROR) << "listen failed due to: " << ec;
    return -1;
  }

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
  signals.add(SIGTERM, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
  signals.async_wait([&](const asio::error_code& /*error*/,
                         int /*signal_number*/) { factory.stop(); });

  io_context.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
    return -1;
  }

  ::google::ShutdownGoogleLogging();

  return 0;
}
