//
// worker.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "worker.hpp"

using boost::asio::ip::tcp;
using namespace socks5;

static tcp::endpoint resolveEndpoint(boost::asio::io_context &io_context,
                                     const std::string &host, uint16_t port) {
  boost::asio::ip::tcp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port));
  return endpoints->endpoint();
}

Worker::Worker() : thread_(std::bind(&Worker::WorkFunc, this)) {
  endpoint_ = resolveEndpoint(io_context_, FLAGS_local_host, FLAGS_local_port);
  remote_endpoint_ =
      resolveEndpoint(io_context_, FLAGS_server_host, FLAGS_server_port);
}

void Worker::Start() {
  endpoint_ = resolveEndpoint(io_context_, FLAGS_local_host, FLAGS_local_port);
  remote_endpoint_ =
      resolveEndpoint(io_context_, FLAGS_server_host, FLAGS_server_port);

  socks5_server_ = std::make_unique<Socks5Factory>(io_context_);
  socks5_server_->listen(endpoint_, remote_endpoint_);
}

void Worker::Stop() { socks5_server_->stop(); }

void Worker::WorkFunc() {
#if 0
  boost::asio::signal_set signals(io_context_);
  signals.add(SIGINT);
  signals.add(SIGTERM);
  signals.add(SIGQUIT);
  signals.async_wait([&](const boost::system::error_code &error,
                         int signal_number) { factory.stop(); });
#endif

  io_context_.run();
}
