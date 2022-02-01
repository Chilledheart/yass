// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#include "cli/cli_worker.hpp"

#include <absl/flags/flag.h>

#include "core/compiler_specific.hpp"

using namespace socks5;

Worker::Worker()
    : work_guard_(asio::make_work_guard(io_context_)),
      resolver_(io_context_),
      thread_([this] { WorkFunc(); }) {}

Worker::~Worker() {
  Stop(std::function<void()>());
  work_guard_.reset();
  thread_.join();
}

void Worker::Start(std::function<void(asio::error_code)> callback) {
  DCHECK_EQ(socks5_server_.get(), nullptr);
  /// listen in the worker thread
  io_context_.post([this, callback]() {
    resolver_.async_resolve(
        absl::GetFlag(FLAGS_local_host),
        std::to_string(absl::GetFlag(FLAGS_local_port)),
        [this, callback](const asio::error_code& ec,
                         asio::ip::tcp::resolver::results_type results) {
          on_resolve_local(ec, results, callback);
        });
  });
}

void Worker::Stop(std::function<void()> callback) {
  /// stop in the worker thread
  io_context_.post([this, callback]() {
    resolver_.cancel();
    if (socks5_server_) {
      socks5_server_->stop();
      socks5_server_->join();
      socks5_server_.reset();
    }
    if (callback) {
      callback();
    }
  });
}

void Worker::WorkFunc() {
  asio::error_code ec;
  if (!SetThreadName(thread_.native_handle(), "background")) {
    PLOG(WARNING) << "failed to set thread name";
  }
  if (!SetThreadPriority(thread_.native_handle(), ThreadPriority::BACKGROUND)) {
    PLOG(WARNING) << "failed to set thread priority";
  }
  io_context_.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
  }
}

void Worker::on_resolve_local(asio::error_code ec,
                              asio::ip::tcp::resolver::results_type results,
                              std::function<void(asio::error_code)> callback) {
  if (ec) {
    LOG(WARNING) << "local resolved failed due to: " << ec;
    if (callback) {
      callback(ec);
    }
    return;
  }
  endpoint_ = results->endpoint();

  resolver_.async_resolve(
      absl::GetFlag(FLAGS_server_host),
      std::to_string(absl::GetFlag(FLAGS_server_port)),
      [this, callback](const asio::error_code& local_ec,
                       asio::ip::tcp::resolver::results_type local_results) {
        on_resolve_remote(local_ec, local_results, callback);
      });
}

void Worker::on_resolve_remote(asio::error_code ec,
                               asio::ip::tcp::resolver::results_type results,
                               std::function<void(asio::error_code)> callback) {
  if (ec) {
    LOG(WARNING) << "remote resolved failed due to: " << ec;
    if (callback) {
      callback(ec);
    }
    return;
  }
  remote_endpoint_ = results->endpoint();

  socks5_server_ = std::make_unique<Socks5Factory>(remote_endpoint_);

  socks5_server_->listen(endpoint_, SOMAXCONN, ec);

  if (ec) {
    socks5_server_->stop();
    socks5_server_->join();
    socks5_server_.reset();
  }

  if (callback) {
    callback(ec);
  }
}
