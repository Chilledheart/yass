// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "config/config.hpp"
#include "crypto/crypter_export.hpp"
#include "server/server_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_cat.h>
#include <locale.h>
#include <openssl/crypto.h>

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#endif

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "i18n/icu_util.hpp"
#include "net/asio.hpp"
#include "net/resolver.hpp"
#include "version.h"

ABSL_FLAG(std::string, user, "", "set non-privileged user for worker");
ABSL_FLAG(std::string, group, "", "set non-privileged group for worker");

using namespace server;

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
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }

  // Major routine
  // - Read config from ss config file
  // - Listen by local address and local port
  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetProgramUsageMessage(absl::StrCat(
      "Usage: ", Basename(exec_path), " [options ...]\n", " -K, --config <file> Read config from a file\n",
      " --certificate_chain_file <file> (TLS) Certificate Chain File Path\n",
      " --private_key_file <file> (TLS) Private Key File Path\n",
      " --private_key_password <password> (TLS) Private Key Password\n", " --server_host <host> Server on given host\n",
      " --server_port <port> Server on given port\n", " --username <username> Server user\n",
      " --password <pasword> Server password\n", " --method <method> Specify encrypt of method to use"));

  config::ReadConfigFileOption(argc, argv);
  config::ReadConfig();
  absl::ParseCommandLine(argc, const_cast<char**>(argv));

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
  }
#endif

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

  // raise some early warning on SSL server setups
  auto method = absl::GetFlag(FLAGS_method).method;
  if (method == cipher_method::CRYPTO_HTTPS || method == cipher_method::CRYPTO_HTTP2) {
    ssize_t ret;
    std::string private_key, private_key_path = absl::GetFlag(FLAGS_private_key_file);
    if (private_key_path.empty()) {
      LOG(WARNING) << "No private key file for certificate provided";
      return -1;
    }
    private_key.resize(256 * 1024);
    ret = ReadFileToBuffer(private_key_path, private_key.data(), private_key.size());
    if (ret < 0) {
      LOG(WARNING) << "private key " << private_key_path << " failed to read";
      return -1;
    }
    private_key.resize(ret);
    g_private_key_content = private_key;
    VLOG(1) << "Using private key file: " << private_key_path;

    std::string certificate_chain, certificate_chain_path = absl::GetFlag(FLAGS_certificate_chain_file);
    if (certificate_chain_path.empty()) {
      LOG(WARNING) << "No certificate file provided";
      return -1;
    }
    certificate_chain.resize(256 * 1024);
    ret = ReadFileToBuffer(certificate_chain_path, certificate_chain.data(), certificate_chain.size());
    if (ret < 0) {
      LOG(WARNING) << "certificate file " << certificate_chain_path << " failed to read";
      return -1;
    }
    certificate_chain.resize(ret);
    g_certificate_chain_content = certificate_chain;
    VLOG(1) << "Using certificate chain file: " << certificate_chain_path;
  }

  std::vector<asio::ip::tcp::endpoint> endpoints;
  std::string host_name = absl::GetFlag(FLAGS_server_host);
  std::string host_sni = host_name;
  uint16_t port = absl::GetFlag(FLAGS_server_port);

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

  return 0;
}
