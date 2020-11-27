// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef YASS_WORKER
#define YASS_WORKER

#include "core/cipher.hpp"

#include <asio.hpp>
#include <gflags/gflags.h>
#include <memory>
#include <thread>

#include "cli/socks5_factory.hpp"
#include "config/config.hpp"
#include "core/logging.hpp"

class Worker {
public:
  Worker();
  ~Worker();

  void Start(bool quiet);
  void Stop(bool quiet);

  const asio::ip::tcp::endpoint &GetEndpoint() const { return endpoint_; }

  const asio::ip::tcp::endpoint &GetRemoteEndpoint() const {
    return remote_endpoint_;
  }

  size_t currentConnections() const {
    return socks5_server_ ? socks5_server_->currentConnections() : 0;
  }

private:
  void WorkFunc();

  asio::io_context io_context_;
  /// stopping the io_context from running out of work
  asio::executor_work_guard<asio::io_context::executor_type> work_guard_;

  std::unique_ptr<Socks5Factory> socks5_server_;
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::endpoint remote_endpoint_;
  std::thread thread_;
};

#endif // YASS_WORKER
