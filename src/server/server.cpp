// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "config/config.hpp"
#include "crypto/crypter_export.hpp"
#include "server/server_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <locale.h>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#endif

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "net/asio.hpp"
#include "net/resolver.hpp"
#include "version.h"

ABSL_FLAG(std::string, user, "", "set non-privileged user for worker");
ABSL_FLAG(std::string, group, "", "set non-privileged group for worker");

const ProgramType pType = YASS_SERVER;

using namespace net::server;

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

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_OHOS)
  // Set C library locale to make sure CommandLine can parse
  // argument values in the correct encoding and to make sure
  // generated file names (think downloads) are in the file system's
  // encoding.
  setlocale(LC_ALL, "");
  // For numbers we never want the C library's locale sensitive
  // conversion from number to string because the only thing it
  // changes is the decimal separator which is not good enough for
  // the UI and can be harmful elsewhere.
  setlocale(LC_NUMERIC, "C");
#endif

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  config::SetServerUsageMessage(exec_path);
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

  std::vector<asio::ip::tcp::endpoint> endpoints;
  std::string host_name = absl::GetFlag(FLAGS_server_host);
  std::string host_sni = host_name;
  uint16_t port = absl::GetFlag(FLAGS_server_port);
  if (port == 0u) {
    LOG(WARNING) << "Invalid server port: " << port;
    return -1;
  }

  asio::error_code ec;
  auto addr = asio::ip::make_address(host_name.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    endpoints.emplace_back(addr, port);
    host_sni = std::string();
  } else {
    asio::io_context io_context;
    auto work_guard =
        std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());
    net::Resolver resolver(io_context);
    if (resolver.Init() < 0) {
      LOG(WARNING) << "Resolver: Init failure";
      return -1;
    }
    asio::ip::tcp::resolver::results_type results;
    resolver.AsyncResolve(host_name, port, [&](asio::error_code ec, asio::ip::tcp::resolver::results_type _results) {
      work_guard.reset();
      if (ec) {
        LOG(WARNING) << "resolved domain name: " << host_name << " failed due to: " << ec;
        return;
      }
      results = std::move(_results);
    });
    io_context.run();
    endpoints.insert(endpoints.end(), std::begin(results), std::end(results));
  }

  if (!absl::GetFlag(FLAGS_server_sni).empty()) {
    host_sni = absl::GetFlag(FLAGS_server_sni);
  }
  if (host_sni.size() > TLSEXT_MAXLEN_host_name) {
    LOG(WARNING) << "Invalid server name or SNI: " << host_sni;
    return -1;
  }

  ServerServer server(io_context);
  for (auto& endpoint : endpoints) {
    server.listen(endpoint, host_sni, SOMAXCONN, ec);
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
  signals.add(SIGTERM, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
#if defined(SIGUSR1)
  signals.add(SIGUSR1, ec);
#endif
  std::function<void(asio::error_code, int)> cb;
  cb = [&](asio::error_code /*ec*/, int signal_number) {
#if defined(SIGUSR1)
    if (signal_number == SIGUSR1) {
      PrintMallocStats();
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

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));
#endif

#ifndef _WIN32
  // change user and change group
  std::string username = absl::GetFlag(FLAGS_user);
  std::string groupname = absl::GetFlag(FLAGS_group);
  if ((!username.empty()) && ::geteuid() == 0) {
    int uid = 0;
    int gid = 0;
    char buffer[PATH_MAX * 2] = {'\0'};

    if (!username.empty()) {
      struct passwd pwd;
      struct passwd* result = nullptr;
      int pwnam_res = getpwnam_r(username.c_str(), &pwd, buffer, sizeof(buffer), &result);
      if (pwnam_res == 0 && result) {
        uid = result->pw_uid;
      } else {
        PLOG(WARNING) << "Failed to find user named: " << username;
        return -1;
      }
    }

    if (!groupname.empty()) {
      struct group grp;
      struct group* grp_result = nullptr;
      int pwnam_gres = getgrnam_r(groupname.c_str(), &grp, buffer, sizeof(buffer), &grp_result);
      if (pwnam_gres == 0 && grp_result) {
        gid = grp_result->gr_gid;
      } else {
        PLOG(WARNING) << "Failed to find group named: " << groupname;
        return -1;
      }
    }

    int ret;
    ret = setgid(gid);
    if (ret != 0) {
      PLOG(WARNING) << "setgid failed: to " << gid;
      return -1;
    }
    ret = initgroups(username.c_str(), gid);
    if (ret != 0) {
      PLOG(WARNING) << "initgroups failed: to " << gid;
      return -1;
    }

    ret = setuid(uid);
    if (ret != 0) {
      PLOG(WARNING) << "setuid failed to " << uid;
      return -1;
    }
    LOG(INFO) << "Changed to user: " << username;
    LOG(INFO) << "Changed to group: " << (groupname.empty() ? std::to_string(gid) : groupname);
  }
#endif

#ifdef __linux__
  /* allow coredump after setuid() in Linux 2.4.x */
  if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
    PLOG(WARNING) << "prctl(PR_SET_DUMPABLE) failed";
  }
#endif

  io_context.run();

  PrintMallocStats();

  return 0;
}
