// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#include "cli/cli_worker.hpp"
#include "cli/socks5_server.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>

#include "core/compiler_specific.hpp"

using namespace socks5;

class WorkerPrivate {
 public:
  std::unique_ptr<Socks5Server> socks5_server;
};

Worker::Worker()
    : resolver_(CAresResolver::Create(io_context_)),
      private_(new WorkerPrivate) {
  int ret = resolver_->Init(1000, 5);
  CHECK_EQ(ret, 0) << "c-ares initialize failure";
  static_cast<void>(ret);
}

Worker::~Worker() {
  Stop(std::function<void()>());
  work_guard_.reset();
  delete private_;
}

void Worker::Start(std::function<void(asio::error_code)> callback) {
  DCHECK_EQ(private_->socks5_server.get(), nullptr);
  if (thread_ && thread_->joinable())
    thread_->join();

  /// listen in the worker thread
  thread_ = std::make_unique<std::thread>([this] { WorkFunc(); });
  io_context_.post([this]() {
    if (!SetThreadName(thread_->native_handle(), "background")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetThreadPriority(thread_->native_handle(), ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
  });
  io_context_.post([this, callback]() {
    resolver_->AsyncResolve(absl::GetFlag(FLAGS_local_host),
      std::to_string(absl::GetFlag(FLAGS_local_port)),
      [this, callback](const asio::error_code& ec,
                       asio::ip::tcp::resolver::results_type results) {
        on_resolve_local(ec, results, callback);
    });
  });
}

void Worker::Stop(std::function<void()> callback) {
  /// stop in the worker thread
  if (!thread_) {
    return;
  }
  io_context_.post([this, callback]() {
    resolver_->Cancel();
    if (private_->socks5_server) {
      private_->socks5_server->stop();
    }
    work_guard_.reset();
    if (callback) {
      callback();
    }
  });
  thread_->join();
  private_->socks5_server.reset();
  thread_.reset();
}

size_t Worker::currentConnections() const {
  return private_->socks5_server ? private_->socks5_server->num_of_connections() : 0;
}

std::string Worker::GetDomain() const {
  return absl::StrCat(absl::GetFlag(FLAGS_local_host),
                      ":", std::to_string(absl::GetFlag(FLAGS_local_port)));
}

std::string Worker::GetRemoteDomain() const {
  return absl::StrCat(absl::GetFlag(FLAGS_server_host),
                      ":", std::to_string(absl::GetFlag(FLAGS_server_port)));
}

void Worker::WorkFunc() {
  asio::error_code ec;
  VLOG(1) << "background thread started";

  work_guard_ = std::make_unique<asio::io_context::work>(io_context_);
  io_context_.run(ec);
  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
  }
  io_context_.reset();

  VLOG(1) << "background thread stopped";
}

void Worker::on_resolve_local(asio::error_code ec,
                              asio::ip::tcp::resolver::results_type results,
                              std::function<void(asio::error_code)> callback) {
  if (ec) {
    LOG(WARNING) << "local resolved failed due to: " << ec;
    if (callback) {
      callback(ec);
    }
    work_guard_.reset();
    return;
  }
  endpoint_ = results->endpoint();

  private_->socks5_server = std::make_unique<Socks5Server>(io_context_,
                                                           absl::GetFlag(FLAGS_server_host),
                                                           absl::GetFlag(FLAGS_server_port)
                                                           );

  private_->socks5_server->listen(endpoint_, SOMAXCONN, ec);
  endpoint_ = private_->socks5_server->endpoint();

  if (ec) {
    private_->socks5_server->stop();
  }

  work_guard_.reset();
  if (callback) {
    callback(ec);
  }
}
