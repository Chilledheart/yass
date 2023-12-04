// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#include "cli/cli_worker.hpp"
#include "cli/cli_server.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#include "core/compiler_specific.hpp"

using namespace cli;

class WorkerPrivate {
 public:
  std::unique_ptr<CliServer> cli_server;
};

Worker::Worker()
#ifdef HAVE_C_ARES
    : resolver_(CAresResolver::Create(io_context_)),
#else
    : resolver_(io_context_),
#endif
      private_(new WorkerPrivate) {
#ifdef HAVE_C_ARES
  int ret = resolver_->Init(5000);
  CHECK_EQ(ret, 0) << "c-ares initialize failure";
  static_cast<void>(ret);
#endif

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();
}

Worker::~Worker() {
  start_callback_ = nullptr;
  stop_callback_ = nullptr;
  Stop(nullptr);

  if (thread_ && thread_->joinable())
    thread_->join();

  delete private_;
}

void Worker::Start(absl::AnyInvocable<void(asio::error_code)> &&callback) {
  if (thread_ && thread_->joinable())
    thread_->join();
  DCHECK(!start_callback_);
  start_callback_ = std::move(callback);
  DCHECK_EQ(private_->cli_server.get(), nullptr);

  /// listen in the worker thread
  thread_ = std::make_unique<std::thread>([this] { WorkFunc(); });
  asio::post(io_context_, [this]() {
    if (!SetThreadName(thread_->native_handle(), "background")) {
      PLOG(WARNING) << "failed to set thread name";
    }
    if (!SetThreadPriority(thread_->native_handle(), ThreadPriority::ABOVE_NORMAL)) {
      PLOG(WARNING) << "failed to set thread priority";
    }
  });
  asio::post(io_context_, [this]() {
    std::string host_name = absl::GetFlag(FLAGS_local_host);
    uint16_t port = absl::GetFlag(FLAGS_local_port);

    asio::error_code ec;
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
  });
}

void Worker::Stop(absl::AnyInvocable<void()> &&callback) {
  DCHECK(!stop_callback_);
  stop_callback_ = std::move(callback);
  /// stop in the worker thread
  if (!thread_) {
    return;
  }
  asio::post(io_context_ ,[this]() {
#ifdef HAVE_C_ARES
    resolver_->Cancel();
#else
    resolver_.cancel();
#endif

    if (private_->cli_server) {
      private_->cli_server->stop();
    }
    work_guard_.reset();
  });
}

size_t Worker::currentConnections() const {
  return private_->cli_server ? private_->cli_server->num_of_connections() : 0;
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

  work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
  io_context_.run();
  io_context_.restart();
  private_->cli_server.reset();

  auto callback = std::move(stop_callback_);
  if (callback) {
    callback();
  }
  DCHECK(!stop_callback_);

  VLOG(1) << "background thread stopped";
}

void Worker::on_resolve_local(asio::error_code ec,
                              asio::ip::tcp::resolver::results_type results) {
  auto callback = std::move(start_callback_);
  if (ec) {
    LOG(WARNING) << "local resolved host:" << absl::GetFlag(FLAGS_local_host)
      << " failed due to: " << ec;
    if (callback) {
      callback(ec);
    }
    work_guard_.reset();
    return;
  }
  endpoints_.clear();
  endpoints_.insert(endpoints_.end(), std::begin(results), std::end(results));

  private_->cli_server = std::make_unique<CliServer>(io_context_,
                                                     absl::GetFlag(FLAGS_server_host),
                                                     absl::GetFlag(FLAGS_server_port)
                                                     );

  for (auto &endpoint : endpoints_) {
    private_->cli_server->listen(endpoint, std::string(), SOMAXCONN, ec);
    if (ec) {
      break;
    }
    endpoint = private_->cli_server->endpoint();
    LOG(WARNING) << "tcp server listening at " << endpoint;
  }

  if (ec) {
    private_->cli_server->stop();
    work_guard_.reset();
  }

  if (callback) {
    callback(ec);
  }
}
