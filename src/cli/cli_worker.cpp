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

  start_callback_ = std::move(callback);

  /// listen in the worker thread
  asio::post(io_context_, [this]() {
    DCHECK_EQ(private_->cli_server.get(), nullptr);

#ifdef HAVE_C_ARES
    resolver_ = CAresResolver::Create(io_context_);
    int ret = resolver_->Init(5000);
    if (ret < 0) {
      LOG(WARNING) << "CAresResolver::Init failed";
      if (auto callback = std::move(start_callback_)) {
        callback(asio::error::connection_refused);
      }
      return;
    }
#endif

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

void Worker::WorkFunc() {
  if (!SetCurrentThreadName("background")) {
    PLOG(WARNING) << "failed to set thread name";
  }
  if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
    PLOG(WARNING) << "failed to set thread priority";
  }

  LOG(INFO) << "background thread started";
  while (!in_destroy_) {
    work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
    io_context_.run();
    io_context_.restart();
    private_->cli_server.reset();

#ifdef HAVE_C_ARES
    resolver_.reset();
#endif

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
