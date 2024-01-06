// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#ifndef YASS_CLI_WORKER
#define YASS_CLI_WORKER

#include "core/cipher.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

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

  std::vector<std::string> GetRemoteIpsV4() const;
  std::vector<std::string> GetRemoteIpsV6() const;
  std::string GetDomain() const;
  std::string GetRemoteDomain() const;
  int GetLocalPort() const;

  size_t currentConnections() const;

 public:
  static
  std::string SaveConfig(const std::string& server_host,
                         const std::string& server_sni,
                         const std::string& server_port,
                         const std::string& username,
                         const std::string& password,
                         cipher_method method,
                         const std::string& local_host,
                         const std::string& local_port,
                         const std::string& connect_timeout);

  static
  std::string SaveConfig(const std::string& server_host,
                         const std::string& server_sni,
                         const std::string& server_port,
                         const std::string& username,
                         const std::string& password,
                         const std::string& method_string,
                         const std::string& local_host,
                         const std::string& local_port,
                         const std::string& connect_timeout);

 private:
  void WorkFunc();

  void on_resolve_remote(asio::error_code ec,
                         asio::ip::tcp::resolver::results_type results);

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
  std::string remote_server_ips_;
  std::vector<std::string> remote_server_ips_v4_;
  std::vector<std::string> remote_server_ips_v6_;
  std::string remote_server_sni_;
  int local_port_;
  std::vector<asio::ip::tcp::endpoint> endpoints_;
  std::atomic_bool in_destroy_ = false;
};

#endif  // YASS_CLI_WORKER
