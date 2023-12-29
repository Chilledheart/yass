// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#ifndef YASS_CLI_WORKER
#define YASS_CLI_WORKER

#include "core/cipher.hpp"

#include <atomic>
#include <memory>
#include <thread>

#include <absl/functional/any_invocable.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"

#ifdef HAVE_C_ARES
#include "core/c-ares.hpp"
#endif

class WorkerPrivate;
class Worker {
 public:
  Worker();
  ~Worker();

  void Start(absl::AnyInvocable<void(asio::error_code)> &&callback);
  void Stop(absl::AnyInvocable<void()> &&callback);

  std::string GetDomain() const;
  std::string GetRemoteDomain() const;

  size_t currentConnections() const;

 private:
  void WorkFunc();

  void on_resolve_local(asio::error_code ec,
                        asio::ip::tcp::resolver::results_type results);

  asio::io_context io_context_;
  /// stopping the io_context from running out of work
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
  /// used to resolve local and remote endpoint
#ifdef HAVE_C_ARES
  scoped_refptr<CAresResolver> resolver_;
#else
  asio::ip::tcp::resolver resolver_;
#endif
  /// used to do io in another thread
  std::unique_ptr<std::thread> thread_;

  absl::AnyInvocable<void(asio::error_code)> start_callback_;
  absl::AnyInvocable<void()> stop_callback_;

  WorkerPrivate *private_;
  std::vector<asio::ip::tcp::endpoint> endpoints_;
  std::atomic_bool in_destroy_ = false;
};

#endif  // YASS_CLI_WORKER
