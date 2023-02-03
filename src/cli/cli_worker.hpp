// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#ifndef YASS_CLI_WORKER
#define YASS_CLI_WORKER

#include "core/cipher.hpp"

#include <functional>
#include <memory>
#include <thread>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/c-ares.hpp"
#include "core/logging.hpp"

class WorkerPrivate;
class Worker {
 public:
  Worker();
  ~Worker();

  void Start(std::function<void(asio::error_code)> callback);
  void Stop(std::function<void()> callback);

  std::string GetDomain() const;
  std::string GetRemoteDomain() const;

  size_t currentConnections() const;

 private:
  void WorkFunc();

  void on_resolve_local(asio::error_code ec,
                        asio::ip::tcp::resolver::results_type results,
                        std::function<void(asio::error_code)> callback);

  asio::io_context io_context_;
  /// stopping the io_context from running out of work
  std::unique_ptr<asio::io_context::work> work_guard_;
  /// used to resolve local and remote endpoint
  scoped_refptr<CAresResolver> resolver_;
  /// used to do io in another thread
  std::unique_ptr<std::thread> thread_;

  WorkerPrivate *private_;
  asio::ip::tcp::endpoint endpoint_;
};

#endif  // YASS_CLI_WORKER
