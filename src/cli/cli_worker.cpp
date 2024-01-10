// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#include "cli/cli_worker.hpp"
#include "cli/cli_server.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <openssl/crypto.h>
#include <sstream>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#ifndef _POSIX_HOST_NAME_MAX
#define _POSIX_HOST_NAME_MAX 255
#endif

using namespace cli;

class WorkerPrivate {
 public:
  std::unique_ptr<CliServer> cli_server;
};

Worker::Worker()
#ifdef HAVE_C_ARES
    : resolver_(nullptr),
#else
    : resolver_(io_context_),
#endif
      private_(new WorkerPrivate) {
#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();

  thread_ = std::make_unique<std::thread>([this] { WorkFunc(); });

  asio::post(io_context_, [this]() {
    if (!SetThreadName(thread_->native_handle(), "background")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetThreadPriority(thread_->native_handle(), ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
  });
}

Worker::~Worker() {
  start_callback_ = nullptr;
  stop_callback_ = nullptr;
  in_destroy_ = true;

  Stop(nullptr);
  thread_->join();

  delete private_;
}

void Worker::Start(absl::AnyInvocable<void(asio::error_code)> &&callback) {
  DCHECK(!start_callback_);

#ifdef HAVE_C_ARES
  resolver_ = CAresResolver::Create(io_context_);
  int ret = resolver_->Init(5000);
  if (ret < 0) {
    LOG(WARNING) << "CAresResolver::Init failed";
    callback(asio::error::connection_refused);
    return;
  }
#endif

  start_callback_ = std::move(callback);

  /// listen in the worker thread
  asio::post(io_context_, [this]() {
    DCHECK_EQ(private_->cli_server.get(), nullptr);

    std::string host_name = absl::GetFlag(FLAGS_server_host);
    uint16_t port = absl::GetFlag(FLAGS_server_port);
    remote_server_sni_ = absl::GetFlag(FLAGS_server_host);
    if (!absl::GetFlag(FLAGS_server_sni).empty()) {
      remote_server_sni_ = absl::GetFlag(FLAGS_server_sni);
    }

    asio::error_code ec;
    auto addr = asio::ip::make_address(host_name.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (host_is_ip_address) {
      asio::ip::tcp::endpoint endpoint(addr, port);
      auto results = asio::ip::tcp::resolver::results_type::create(
        endpoint, host_name, std::to_string(port));
      on_resolve_remote(ec, results);
      return;
    }
#ifdef HAVE_C_ARES
    resolver_->AsyncResolve(host_name, std::to_string(port),
#else
    resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(),
                            host_name, std::to_string(port),
#endif
      [this](const asio::error_code& ec,
             asio::ip::tcp::resolver::results_type results) {
        on_resolve_remote(ec, results);
    });
  });
}

void Worker::Stop(absl::AnyInvocable<void()> &&callback) {
  DCHECK(!stop_callback_);
  stop_callback_ = std::move(callback);
  /// stop in the worker thread
  asio::post(io_context_ ,[this]() {
#ifdef HAVE_C_ARES
    if (resolver_) {
      resolver_->Cancel();
    }
#else
    resolver_.cancel();
#endif

    if (private_->cli_server) {
      LOG(INFO) << "tcp server stops listen";
      private_->cli_server->stop();
    }

    work_guard_.reset();
  });
}

size_t Worker::currentConnections() const {
  return private_->cli_server ? private_->cli_server->num_of_connections() : 0;
}

std::vector<std::string> Worker::GetRemoteIpsV4() const {
  return remote_server_ips_v4_;
}

std::vector<std::string> Worker::GetRemoteIpsV6() const {
  return remote_server_ips_v6_;
}

std::string Worker::GetDomain() const {
  return absl::StrCat(absl::GetFlag(FLAGS_local_host),
                      ":", std::to_string(absl::GetFlag(FLAGS_local_port)));
}

std::string Worker::GetRemoteDomain() const {
  return absl::StrCat(absl::GetFlag(FLAGS_server_host),
                      ":", std::to_string(absl::GetFlag(FLAGS_server_port)));
}

int Worker::GetLocalPort() const {
  return local_port_;
}

std::string
Worker::SaveConfig(const std::string& server_host,
                   const std::string& server_sni,
                   const std::string& _server_port,
                   const std::string& username,
                   const std::string& password,
                   cipher_method method,
                   const std::string& local_host,
                   const std::string& _local_port,
                   const std::string& _timeout) {
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  auto server_port = StringToIntegerU(_server_port);
  if (!server_port.has_value() || server_port.value() > 65535u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << to_cipher_method_str(method);
  }

  if (local_host.empty() || local_host.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  auto local_port = StringToIntegerU(_local_port);
  if (!local_port.has_value() || local_port.value() > 65535u) {
    err_msg << ",Invalid Local Port: " << _local_port;
  }

  auto timeout = StringToIntegerU(_timeout);
  if (!timeout.has_value()) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port.value());
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port.value());
    absl::SetFlag(&FLAGS_connect_timeout, timeout.value());
  } else {
    ret = ret.substr(1);
  }
  return ret;
}

std::string
Worker::SaveConfig(const std::string& server_host,
                   const std::string& server_sni,
                   const std::string& _server_port,
                   const std::string& username,
                   const std::string& password,
                   const std::string& method_string,
                   const std::string& local_host,
                   const std::string& _local_port,
                   const std::string& _timeout) {
  std::ostringstream err_msg;

  if (server_host.empty() || server_host.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Server Host: " << server_host;
  }

  if (server_sni.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Server Host: " << server_sni;
  }

  auto server_port = StringToIntegerU(_server_port);
  if (!server_port.has_value() || server_port.value() > 65535u) {
    err_msg << ",Invalid Server Port: " << _server_port;
  }

  auto method = to_cipher_method(method_string);
  if (method == CRYPTO_INVALID) {
    err_msg << ",Invalid Cipher: " << method_string;
  }

  if (local_host.empty() || local_host.size() >= _POSIX_HOST_NAME_MAX) {
    err_msg << ",Invalid Local Host: " << local_host;
  }

  auto local_port = StringToIntegerU(_local_port);
  if (!local_port.has_value() || local_port.value() > 65535u) {
    err_msg << ",Invalid Local Port: " << _local_port;
  }

  auto timeout = StringToIntegerU(_timeout);
  if (!timeout.has_value()) {
    err_msg << ",Invalid Connect Timeout: " << _timeout;
  }

  std::string ret = err_msg.str();
  if (ret.empty()) {
    absl::SetFlag(&FLAGS_server_host, server_host);
    absl::SetFlag(&FLAGS_server_sni, server_sni);
    absl::SetFlag(&FLAGS_server_port, server_port.value());
    absl::SetFlag(&FLAGS_username, username);
    absl::SetFlag(&FLAGS_password, password);
    absl::SetFlag(&FLAGS_method, method);
    absl::SetFlag(&FLAGS_local_host, local_host);
    absl::SetFlag(&FLAGS_local_port, local_port.value());
    absl::SetFlag(&FLAGS_connect_timeout, timeout.value());
  } else {
    ret = ret.substr(1);
  }
  return ret;
}

void Worker::WorkFunc() {
  LOG(INFO) << "background thread started";
  while (!in_destroy_) {
    work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
    io_context_.run();
    io_context_.restart();
    private_->cli_server.reset();

    auto callback = std::move(stop_callback_);
    DCHECK(!stop_callback_);
    if (callback) {
      callback();
    }
    LOG(INFO) << "background thread finished cleanup";
  }
  LOG(INFO) << "background thread stopped";
}

void Worker::on_resolve_remote(asio::error_code ec,
                               asio::ip::tcp::resolver::results_type results) {
  if (ec) {
    LOG(WARNING) << "remote resolved host:" << absl::GetFlag(FLAGS_server_host)
      << " failed due to: " << ec;
    if (auto callback = std::move(start_callback_)) {
      callback(ec);
    }
    work_guard_.reset();
    return;
  }

  std::vector<std::string> server_ips;
  for (auto result : results) {
    if (result.endpoint().address().is_unspecified()) {
      LOG(WARNING) << "Unspecified remote address: " << absl::GetFlag(FLAGS_server_host);
      ec = asio::error::connection_refused;
      if (auto callback = std::move(start_callback_)) {
        callback(ec);
      }
      work_guard_.reset();
      return;
    }
    server_ips.push_back(result.endpoint().address().to_string());
    if (result.endpoint().address().is_v4()) {
      remote_server_ips_v4_.push_back(result.endpoint().address().to_string());
    } else {
      remote_server_ips_v6_.push_back(result.endpoint().address().to_string());
    }
  }
  remote_server_ips_ = absl::StrJoin(server_ips, ";");
  LOG(INFO) << "resolved server ips: " << remote_server_ips_;

  std::string host_name = absl::GetFlag(FLAGS_local_host);
  uint16_t port = absl::GetFlag(FLAGS_local_port);

  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    asio::ip::tcp::endpoint endpoint(addr, port);
    auto results = asio::ip::tcp::resolver::results_type::create(
      endpoint, host_name, std::to_string(port));
    on_resolve_local(ec, results);
    return;
  }
#ifdef HAVE_C_ARES
  resolver_->AsyncResolve(host_name, std::to_string(port),
#else
  resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(),
                          host_name, std::to_string(port),
#endif
    [this](const asio::error_code& ec,
           asio::ip::tcp::resolver::results_type results) {
      on_resolve_local(ec, results);
  });
}

void Worker::on_resolve_local(asio::error_code ec,
                              asio::ip::tcp::resolver::results_type results) {
  if (ec) {
    LOG(WARNING) << "local resolved host:" << absl::GetFlag(FLAGS_local_host)
      << " failed due to: " << ec;
    if (auto callback = std::move(start_callback_)) {
      callback(ec);
    }
    work_guard_.reset();
    return;
  }
  endpoints_.clear();
  endpoints_.insert(endpoints_.end(), std::begin(results), std::end(results));

  std::vector<std::string> local_ips;
  for (auto result : results) {
    local_ips.push_back(result.endpoint().address().to_string());
  }
  LOG(INFO) << "resolved local ips: " << absl::StrJoin(local_ips, ";");

  private_->cli_server = std::make_unique<CliServer>(io_context_,
                                                     remote_server_ips_,
                                                     remote_server_sni_,
                                                     absl::GetFlag(FLAGS_server_port)
                                                     );

  local_port_ = 0;
  for (auto &endpoint : endpoints_) {
    private_->cli_server->listen(endpoint, std::string(), SOMAXCONN, ec);
    if (ec) {
      break;
    }
    endpoint = private_->cli_server->endpoint();
    local_port_ = endpoint.port();
    LOG(INFO) << "tcp server listening at " << endpoint;
  }

  if (ec) {
    LOG(WARNING) << "tcp server stops listen due to error: " << ec;
    private_->cli_server->stop();
    work_guard_.reset();
  }

  if (auto callback = std::move(start_callback_)) {
    callback(ec);
  }
}
