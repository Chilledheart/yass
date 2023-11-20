// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "cli/cli_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_cat.h>
#include <locale.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV  0x00000008
#endif
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
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

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
  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetProgramUsageMessage(
      absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n",
                   " -c, --configfile <file> Use specified config file\n",
                   " --server_host <host> Host address which remote server listens to\n",
                   " --server_port <port> Port number which remote server listens to\n",
                   " --local_host <host> Host address which local server listens to\n"
                   " --local_port <port> Port number which local server listens to\n"
                   " --username <username> Username\n",
                   " --password <pasword> Password pharsal\n",
                   " --method <method> Method of encrypt"));

  config::ReadConfigFileOption(argc, argv);
  config::ReadConfig();
  absl::ParseCommandLine(argc, const_cast<char**>(argv));

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
  std::string host_name = absl::GetFlag(FLAGS_local_host);
  uint16_t port = absl::GetFlag(FLAGS_local_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    endpoints.emplace_back(addr, port);
  } else {
    struct addrinfo hints = {}, *addrinfo;
    hints.ai_flags = AI_CANONNAME | AI_NUMERICSERV;
    hints.ai_family = Net_ipv6works() ? AF_UNSPEC : AF_INET;
    hints.ai_socktype = SOCK_STREAM;
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
    server.listen(endpoint, std::string(), SOMAXCONN, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      server.stop();
      work_guard.reset();
      return -1;
    }
    endpoint = server.endpoint();
    LOG(WARNING) << "tcp server listening at " << endpoint
      << " with upstream " << remote_domain;
  }

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
  signals.add(SIGTERM, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
#ifdef HAVE_TCMALLOC
  signals.add(SIGUSR1, ec);
#endif
  std::function<void(asio::error_code, int)> cb;
  cb = [&](asio::error_code /*ec*/, int signal_number) {
#ifdef HAVE_TCMALLOC
    if (signal_number == SIGUSR1) {
      PrintTcmallocStats();
      signals.async_wait(cb);
      return;
    }
#endif
#ifdef SIGQUIT
    if (signal_number == SIGQUIT) {
      LOG(WARNING) << "Application shuting down";
      server.shutdown();
    } else {
#endif
      LOG(WARNING) << "Application exiting";
      server.stop();
#ifdef SIGQUIT
    }
#endif
    work_guard.reset();
    signals.clear();
  };
  signals.async_wait(cb);

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  io_context.run();

  return 0;
}
