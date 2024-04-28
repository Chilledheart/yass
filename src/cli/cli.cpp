// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "cli/cli_server.hpp"
#include "config/config.hpp"
#include "crypto/crypter_export.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <build/build_config.h>
#include <locale.h>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "net/asio.hpp"
#include "net/resolver.hpp"
#include "version.h"

const ProgramType pType = YASS_CLIENT;

using namespace cli;

static asio::ip::tcp::resolver::results_type ResolveAddress(const std::string& domain_name, int port) {
  asio::error_code ec;
  auto addr = asio::ip::make_address(domain_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    asio::ip::tcp::endpoint endpoint(addr, port);
    auto results = asio::ip::tcp::resolver::results_type::create(endpoint, domain_name, std::to_string(port));
    return results;
  } else {
    asio::io_context io_context;
    auto work_guard =
        std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());
    net::Resolver resolver(io_context);
    if (resolver.Init() < 0) {
      LOG(WARNING) << "Resolver: Init failure";
      return {};
    }
    asio::ip::tcp::resolver::results_type results;
    resolver.AsyncResolve(domain_name, port, [&](asio::error_code ec, asio::ip::tcp::resolver::results_type _results) {
      work_guard.reset();
      if (ec) {
        LOG(WARNING) << "resolved domain name: " << domain_name << " failed due to: " << ec;
        return;
      }
      results = std::move(_results);
    });
    io_context.run();

    return results;
  }
}

int main(int argc, const char* argv[]) {
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

#ifdef _WIN32
  if (!EnableSecureDllLoading()) {
    return -1;
  }
#endif

  setlocale(LC_ALL, "");

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  config::SetClientUsageMessage(exec_path);
  config::ReadConfigFileAndArguments(argc, argv);

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();

  // Start Io Context
  asio::io_context io_context;
  auto work_guard =
      std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

  std::string remote_host_name = absl::GetFlag(FLAGS_server_host);
  std::string remote_host_sni = remote_host_name;
  if (!absl::GetFlag(FLAGS_server_sni).empty()) {
    remote_host_sni = absl::GetFlag(FLAGS_server_sni);
  }
  if (remote_host_sni.size() > TLSEXT_MAXLEN_host_name) {
    LOG(WARNING) << "Invalid server name or SNI: " << remote_host_sni;
    return -1;
  }

  std::string remote_host_ips;
  uint16_t remote_port = absl::GetFlag(FLAGS_server_port);
  if (remote_port == 0u) {
    LOG(WARNING) << "Invalid server port: " << remote_port;
    return -1;
  }

  auto results = ResolveAddress(remote_host_name, remote_port);
  if (results.empty()) {
    return -1;
  } else {
    std::vector<std::string> remote_ips;
    for (auto result : results) {
      if (result.endpoint().address().is_unspecified()) {
        LOG(WARNING) << "Unspecified remote address: " << remote_host_name;
        return -1;
      }
      remote_ips.push_back(result.endpoint().address().to_string());
    }
    remote_host_ips = absl::StrJoin(remote_ips, ";");
    LOG(INFO) << "resolved server ips: " << remote_host_ips << " from " << remote_host_name;
  }

  std::vector<asio::ip::tcp::endpoint> endpoints;
  std::string local_host_name = absl::GetFlag(FLAGS_local_host);
  uint16_t local_port = absl::GetFlag(FLAGS_local_port);

  results = ResolveAddress(local_host_name, local_port);

  if (results.empty()) {
    return -1;
  } else {
    endpoints.insert(endpoints.end(), std::begin(results), std::end(results));

    std::vector<std::string> local_ips;
    for (auto result : results) {
      local_ips.push_back(result.endpoint().address().to_string());
    }
    LOG(INFO) << "resolved local ips: " << absl::StrJoin(local_ips, ";") << " from " << local_host_name;
  }

  asio::error_code ec;
  CliServer server(io_context, remote_host_ips, remote_host_sni, remote_port);
  for (auto& endpoint : endpoints) {
    server.listen(endpoint, std::string(), SOMAXCONN, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      server.stop();
      work_guard.reset();
      return -1;
    }
    endpoint = server.endpoint();
    LOG(WARNING) << "tcp server listening at " << endpoint << " with upstream sni: " << remote_host_sni << ":"
                 << remote_port << " (ip " << remote_host_ips << " )";
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
