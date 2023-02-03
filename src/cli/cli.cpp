// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "config/config.hpp"
#include "core/cipher.hpp"
#include "cli/socks5_server.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_cat.h>
#include <locale.h>

#include "core/asio.hpp"
#include "core/logging.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"

using namespace socks5;

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
  auto work_guard = std::make_unique<asio::io_context::work>(io_context);

  asio::ip::tcp::resolver resolver(io_context);
  asio::error_code ec;

  auto endpoints = resolver.resolve(absl::GetFlag(FLAGS_local_host),
      std::to_string(absl::GetFlag(FLAGS_local_port)), ec);
  if (ec) {
    LOG(WARNING) << "local resolved failed due to: " << ec;
    return -1;
  }
  asio::ip::tcp::endpoint endpoint = endpoints->endpoint();

  std::string remote_domain = absl::StrCat(absl::GetFlag(FLAGS_server_host),
    ":", absl::GetFlag(FLAGS_server_port));

  LOG(WARNING) << "using " << endpoint << " with upstream " << remote_domain;

  Socks5Server server(io_context, absl::GetFlag(FLAGS_server_host),
                      absl::GetFlag(FLAGS_server_port));
  server.listen(endpoint, SOMAXCONN, ec);
  if (ec) {
    LOG(ERROR) << "listen failed due to: " << ec;
    server.stop();
    work_guard.reset();
    return -1;
  }
  endpoint = server.endpoint();

  asio::signal_set signals(io_context);
  signals.add(SIGINT, ec);
#ifdef SIGQUIT
  signals.add(SIGQUIT, ec);
#endif
  signals.async_wait([&](asio::error_code /*error*/, int /*signal_number*/) {
    server.stop();
    work_guard.reset();
  });

  io_context.run(ec);

  if (ec) {
    LOG(ERROR) << "io_context failed due to: " << ec;
    return -1;
  }

  return 0;
}
