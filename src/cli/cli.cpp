//
// cli.cpp
// ~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "cli/socks5_factory.hpp"

#include <asio.hpp>
#include <gflags/gflags.h>

#include "core/logging.hpp"

using namespace socks5;

asio::ip::tcp::endpoint resolveEndpoint(asio::io_context *io_context,
                                               const std::string &host, uint16_t port) {
  asio::error_code ec = asio::error_code();
  asio::ip::tcp::resolver resolver(*io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  return endpoints->endpoint();
}

int main(int argc, const char *argv[]) {
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
  ::FLAGS_v = 2;
#else
  ::FLAGS_logbuflevel = 1;
  ::FLAGS_v = 1;
#endif
  ::google::ParseCommandLineFlags(&argc, (char ***)&argv, true);
  ::google::InstallFailureSignalHandler();

  (void)config::ReadConfig();

  asio::ip::tcp::endpoint endpoint(
      resolveEndpoint(&io_context, FLAGS_local_host, FLAGS_local_port));
  asio::ip::tcp::endpoint remoteEndpoint(
      resolveEndpoint(&io_context, FLAGS_server_host, FLAGS_server_port));

  LOG(WARNING) << "using " << endpoint << " with upstream " << remoteEndpoint;

  Socks5Factory factory(io_context, remoteEndpoint);
  asio::error_code ec;
  factory.listen(endpoint, SOMAXCONN, ec);
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
  signals.async_wait([&](const asio::error_code &/*error*/,
                         int /*signal_number*/) {
    factory.stop();
  });

  io_context.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
    return -1;
  }

  ::google::ShutdownGoogleLogging();

  return 0;
}
