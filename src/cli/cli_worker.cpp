// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#include "cli/cli_worker.hpp"
#include "cli/cli_server.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

using namespace cli;

class WorkerPrivate {
 public:
  std::unique_ptr<CliServer> cli_server;
};

Worker::Worker() : resolver_(io_context_), private_(new WorkerPrivate) {
#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();

  thread_ = std::make_unique<std::thread>([this] { WorkFunc(); });
}

Worker::~Worker() {
  start_callback_ = nullptr;
  stop_callback_ = nullptr;
  in_destroy_ = true;

  Stop(nullptr);
  thread_->join();

  delete private_;
}

void Worker::Start(absl::AnyInvocable<void(asio::error_code)>&& callback) {
  DCHECK(!start_callback_);

  start_callback_ = std::move(callback);

  /// listen in the worker thread
  asio::post(io_context_, [this]() {
    DCHECK_EQ(private_->cli_server.get(), nullptr);

    // FIXME handle doh_url as well
#if 0
    // cached dns results
    bool cache_hit = absl::GetFlag(FLAGS_server_host) == cached_server_host_ &&
                     absl::GetFlag(FLAGS_server_sni) == cached_server_sni_ &&
                     absl::GetFlag(FLAGS_server_port) == cached_server_port_ &&
                     absl::GetFlag(FLAGS_local_host) == cached_local_host_ &&
                     absl::GetFlag(FLAGS_local_port) == cached_local_port_;

    if (cache_hit && !remote_server_ips_.empty() && !endpoints_.empty()) {
      DCHECK(!endpoints_.empty());
      LOG(INFO) << "worker: using cached remote ip: " << remote_server_ips_ << " local ip: " << local_server_ips_;
      on_resolve_done({});
      return;
    }
#endif

    // overwrite cached entry
    cached_server_host_ = absl::GetFlag(FLAGS_server_host);
    cached_server_sni_ = absl::GetFlag(FLAGS_server_sni);
    cached_server_port_ = absl::GetFlag(FLAGS_server_port);
    cached_local_host_ = absl::GetFlag(FLAGS_local_host);
    cached_local_port_ = absl::GetFlag(FLAGS_local_port);

    int ret = resolver_.Init();
    if (ret < 0) {
      LOG(WARNING) << "worker: resolver::Init failed";
      on_resolve_done(asio::error::connection_refused);
      return;
    }

    std::string host_name = cached_server_host_;
    uint16_t port = cached_server_port_;
    remote_server_sni_ = cached_server_host_;
    if (!cached_server_sni_.empty()) {
      remote_server_sni_ = cached_server_sni_;
    }

    asio::error_code ec;
    auto addr = asio::ip::make_address(host_name.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (host_is_ip_address) {
      asio::ip::tcp::endpoint endpoint(addr, port);
      auto results = asio::ip::tcp::resolver::results_type::create(endpoint, host_name, std::to_string(port));
      on_resolve_remote(ec, results);
      return;
    }
    resolver_.AsyncResolve(host_name, port,
                           [this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
                             on_resolve_remote(ec, results);
                           });
  });
}

void Worker::Stop(absl::AnyInvocable<void()>&& callback) {
  DCHECK(!stop_callback_);
  stop_callback_ = std::move(callback);
  /// stop in the worker thread
  asio::post(io_context_, [this]() {
    resolver_.Cancel();

    if (private_->cli_server) {
      LOG(INFO) << "worker: tcp server stops listen";
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
  return absl::StrCat(cached_local_host_, ":", std::to_string(cached_local_port_));
}

std::string Worker::GetRemoteDomain() const {
  return absl::StrCat(cached_server_host_, ":", std::to_string(cached_server_port_));
}

int Worker::GetLocalPort() const {
  return local_port_;
}

void Worker::WorkFunc() {
  if (!SetCurrentThreadName("background")) {
    PLOG(WARNING) << "worker: failed to set thread name";
  }
  if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
    PLOG(WARNING) << "worker: failed to set thread priority";
  }

  LOG(INFO) << "worker: background thread started";
  while (!in_destroy_) {
    work_guard_ =
        std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
    io_context_.run();
    io_context_.restart();
    private_->cli_server.reset();

    resolver_.Reset();

    auto callback = std::move(stop_callback_);
    DCHECK(!stop_callback_);
    if (callback) {
      callback();
    }
    LOG(INFO) << "worker: background thread finished cleanup";
  }
  LOG(INFO) << "worker: background thread stopped";
}

void Worker::on_resolve_remote(asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
  if (ec) {
    LOG(WARNING) << "worker: remote resolved host: " << cached_server_host_ << " failed due to: " << ec;
    on_resolve_done(ec);
    return;
  }

  std::vector<std::string> server_ips;
  for (auto result : results) {
    if (result.endpoint().address().is_unspecified()) {
      LOG(WARNING) << "worker: unspecified remote address: " << cached_server_host_;
      on_resolve_done(asio::error::connection_refused);
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
  LOG(INFO) << "worker: resolved server ips: " << remote_server_ips_;

  std::string host_name = cached_local_host_;
  uint16_t port = cached_local_port_;

  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    asio::ip::tcp::endpoint endpoint(addr, port);
    auto results = asio::ip::tcp::resolver::results_type::create(endpoint, host_name, std::to_string(port));
    on_resolve_local(ec, results);
    return;
  }
  resolver_.AsyncResolve(host_name, port,
                         [this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
                           on_resolve_local(ec, results);
                         });
}

void Worker::on_resolve_local(asio::error_code ec, asio::ip::tcp::resolver::results_type results) {
  if (ec) {
    LOG(WARNING) << "worker: local resolved host: " << cached_local_host_ << " failed due to: " << ec;
    on_resolve_done(ec);
    return;
  }
  endpoints_.clear();
  endpoints_.insert(endpoints_.end(), std::begin(results), std::end(results));

  std::vector<std::string> local_ips;
  for (auto result : results) {
    local_ips.push_back(result.endpoint().address().to_string());
  }
  local_server_ips_ = absl::StrJoin(local_ips, ";");
  LOG(INFO) << "worker: resolved local ips: " << local_server_ips_;

  on_resolve_done({});
}

void Worker::on_resolve_done(asio::error_code ec) {
  if (ec) {
    if (auto callback = std::move(start_callback_)) {
      callback(ec);
    }
    work_guard_.reset();
    return;
  }

  private_->cli_server =
      std::make_unique<CliServer>(io_context_, remote_server_ips_, remote_server_sni_, cached_server_port_);

  local_port_ = 0;
  for (auto& endpoint : endpoints_) {
    private_->cli_server->listen(endpoint, std::string(), SOMAXCONN, ec);
    if (ec) {
      break;
    }
    endpoint = private_->cli_server->endpoint();
    local_port_ = endpoint.port();
    LOG(INFO) << "worker: tcp server listening at " << endpoint;
  }

  if (ec) {
    LOG(WARNING) << "worker: tcp server stops listen due to error: " << ec;
    private_->cli_server->stop();
    work_guard_.reset();
  }

  if (auto callback = std::move(start_callback_)) {
    callback(ec);
  }
}
