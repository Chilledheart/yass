// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#include "cli/cli_worker.hpp"

#include <absl/flags/flag.h>
#ifdef __APPLE__
#include <pthread.h>
#endif

using asio::ip::tcp;
using namespace socks5;

namespace {
asio::ip::tcp::endpoint resolveEndpoint(asio::io_context* io_context,
                                        const std::string& host,
                                        uint16_t port,
                                        asio::error_code& ec) {
  asio::ip::tcp::resolver resolver(*io_context);
  auto endpoints = resolver.resolve(host, std::to_string(port), ec);
  if (ec) {
    LOG(WARNING) << "name resolved failed due to: " << ec;
    return asio::ip::tcp::endpoint();
  }
  return endpoints->endpoint();
}
}  // namespace

Worker::Worker()
    : work_guard_(asio::make_work_guard(io_context_)),
      thread_([this] { WorkFunc(); }) {}

Worker::~Worker() {
  Stop([]() {});
  work_guard_.reset();
  thread_.join();
}

void Worker::Start(std::function<void(asio::error_code)> callback) {
  asio::error_code ec;
  endpoint_ = resolveEndpoint(&io_context_, absl::GetFlag(FLAGS_local_host),
                              absl::GetFlag(FLAGS_local_port), ec);
  if (ec) {
    callback(ec);
  }

  remote_endpoint_ =
      resolveEndpoint(&io_context_, absl::GetFlag(FLAGS_server_host),
                      absl::GetFlag(FLAGS_server_port), ec);

  if (ec) {
    callback(ec);
  }

  /// listen in the worker thread
  io_context_.post([this, callback]() {
    asio::error_code local_ec;

    socks5_server_ =
        std::make_unique<Socks5Factory>(io_context_, remote_endpoint_);

    socks5_server_->listen(endpoint_, SOMAXCONN, local_ec);

    if (callback) {
      callback(local_ec);
    }
  });
}

void Worker::Stop(std::function<void()> callback) {
  /// stop in the worker thread
  io_context_.post([this, callback]() {
    if (socks5_server_) {
      socks5_server_->stop();
    }
    if (callback) {
      callback();
    }
  });
}

void Worker::WorkFunc() {
  asio::error_code ec = asio::error_code();
#ifdef __APPLE__
  /// documented in
  /// https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
  /// see sys/qos.h
  pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
  io_context_.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
  }
}
