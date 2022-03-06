// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_CLI_WORKER
#define YASS_CLI_WORKER

#include "core/cipher.hpp"

#include <functional>
#include <memory>
#include <thread>

#include "cli/socks5_factory.hpp"
#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"

class Worker {
 public:
  Worker();
  ~Worker();

  void Start(std::function<void(asio::error_code)> callback);
  void Stop(std::function<void()> callback);

  const asio::ip::tcp::endpoint& GetEndpoint() const { return endpoint_; }

  const asio::ip::tcp::endpoint& GetRemoteEndpoint() const {
    return remote_endpoint_;
  }

  size_t currentConnections() const {
    return socks5_server_ ? socks5_server_->currentConnections() : 0;
  }

 private:
  void WorkFunc();

  void on_resolve_local(asio::error_code ec,
                        asio::ip::tcp::resolver::results_type results,
                        std::function<void(asio::error_code)> callback);
  void on_resolve_remote(asio::error_code ec,
                         asio::ip::tcp::resolver::results_type results,
                         std::function<void(asio::error_code)> callback);

  asio::io_context io_context_;
  /// stopping the io_context from running out of work
  std::unique_ptr<asio::io_context::work> work_guard_;
  /// used to resolve local and remote endpoint
  asio::ip::tcp::resolver resolver_;

  std::unique_ptr<Socks5Factory> socks5_server_;
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::endpoint remote_endpoint_;
  std::thread thread_;
};

#endif  // YASS_CLI_WORKER
