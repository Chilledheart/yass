// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "server/server_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <locale.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "core/asio.hpp"
#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"

using namespace server;

int main(int argc, const char* argv[]) {
#ifdef OS_WIN
  if (!EnableSecureDllLoading()) {
    return -1;
  }
#endif
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::ParseCommandLine(argc, const_cast<char**>(argv));

  auto cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
  }

  config::ReadConfig();
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();

  // Start Io Context
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  std::vector<asio::ip::tcp::endpoint> endpoints;
  std::string host_name = absl::GetFlag(FLAGS_server_host);
  uint16_t port = absl::GetFlag(FLAGS_server_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    endpoints.emplace_back(addr, port);
  } else {
    struct addrinfo hints = {}, *addrinfo;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_INET;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    int ret = ::getaddrinfo(host_name.c_str(), std::to_string(port).c_str(), &hints, &addrinfo);
    auto results = asio::ip::tcp::resolver::results_type::create(addrinfo, host_name.c_str(), std::to_string(port));
    ::freeaddrinfo(addrinfo);
    if (ret) {
      LOG(WARNING) << "server resolved host: " << host_name
#ifdef _WIN32
        << " failed due to: " << gai_strerrorA(ret);
#else
        << " failed due to: " << gai_strerror(ret);
#endif
    }
    endpoints.insert(endpoints.end(), std::begin(results), std::end(results));
  }

  ServerServer server(io_context);
  for (auto &endpoint : endpoints) {
    server.listen(endpoint, SOMAXCONN, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      server.stop();
      work_guard.reset();
      return -1;
    }
    endpoint = server.endpoint();
    LOG(WARNING) << "tcp server listening at " << endpoint;
  }

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
  signals.async_wait([&](asio::error_code /*error*/, int /*signal_number*/) {
    server.stop();
    work_guard.reset();
  });

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  io_context.run();

  return 0;
}
