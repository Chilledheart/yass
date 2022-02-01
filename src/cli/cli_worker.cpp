// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#include "cli/cli_worker.hpp"

#include <absl/flags/flag.h>

#include "core/compiler_specific.hpp"

using namespace socks5;

Worker::Worker()
    : work_guard_(asio::make_work_guard(io_context_)),
      thread_([this] { WorkFunc(); }) {}

Worker::~Worker() {
  Stop([]() {});
  work_guard_.reset();
  thread_.join();
}

void Worker::Start(std::function<void(asio::error_code)> callback) {
  asio::ip::tcp::resolver resolver(io_context_);
  asio::error_code ec;

  auto endpoints = resolver.resolve(absl::GetFlag(FLAGS_local_host),
      std::to_string(absl::GetFlag(FLAGS_local_port)), ec);
  if (ec) {
    LOG(WARNING) << "local resolved failed due to: " << ec;
    callback(ec);
    return;
  }
  endpoint_ = endpoints->endpoint();

  endpoints = resolver.resolve(absl::GetFlag(FLAGS_server_host),
      std::to_string(absl::GetFlag(FLAGS_server_port)), ec);

  if (ec) {
    LOG(WARNING) << "remote resolved failed due to: " << ec;
    callback(ec);
    return;
  }
  remote_endpoint_ = endpoints->endpoint();

  /// listen in the worker thread
  io_context_.post([this, callback]() {
    asio::error_code local_ec;

    socks5_server_ = std::make_unique<Socks5Factory>(remote_endpoint_);

    socks5_server_->listen(endpoint_, SOMAXCONN, local_ec);

    if (local_ec) {
      socks5_server_->stop();
      socks5_server_->join();
    }

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
      socks5_server_->join();
    }
    if (callback) {
      callback();
    }
  });
}

void Worker::WorkFunc() {
  asio::error_code ec = asio::error_code();
  if (!SetThreadName(thread_.native_handle(), "background")) {
    PLOG(WARNING) << "failed to set thread name";
  }
  if (!SetThreadPriority(thread_.native_handle(),
                         ThreadPriority::BACKGROUND)) {
    PLOG(WARNING) << "failed to set thread priority";
  }
  io_context_.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
  }
}
