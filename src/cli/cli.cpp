// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "cli/cli_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_cat.h>
#include <locale.h>

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

using namespace cli;

int main(int argc, const char* argv[]) {
#ifdef OS_WIN
  if (!EnableSecureDllLoading()) {
    return -1;
  }
#endif
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(argv[0]);
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

  // Start Io Context
  asio::io_context io_context;
  auto work_guard = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  std::vector<asio::ip::tcp::endpoint> endpoints;
  std::string host_name = absl::GetFlag(FLAGS_local_host);
  uint16_t port = absl::GetFlag(FLAGS_local_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    endpoints.push_back(asio::ip::tcp::endpoint(addr, port));
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
      LOG(WARNING) << "local resolved host:" << host_name
#ifdef _WIN32
        << " failed due to: " << gai_strerrorA(ret);
#else
        << " failed due to: " << gai_strerror(ret);
#endif
      return -1;
    }
    endpoints.insert(endpoints.end(), std::begin(results), std::end(results));
  }

  std::string remote_domain = absl::StrCat(
    absl::GetFlag(FLAGS_server_host), ":", absl::GetFlag(FLAGS_server_port));


  CliServer server(io_context, absl::GetFlag(FLAGS_server_host),
                   absl::GetFlag(FLAGS_server_port));
  for (auto &endpoint : endpoints) {
    server.listen(endpoint, SOMAXCONN, ec);
    endpoint = server.endpoint();
    LOG(WARNING) << "tcp server listening at " << endpoint
      << " with upstream " << remote_domain;
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      server.stop();
      work_guard.reset();
      return -1;
    }
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
