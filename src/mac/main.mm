// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import <Cocoa/Cocoa.h>

#include <mach-o/dyld.h>
#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"

static std::string GetMainExecutablePath() {
  char exe_path[PATH_MAX];
  uint32_t size = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &size) == 0) {
    char link_path[PATH_MAX];
    if (realpath(exe_path, link_path))
      return link_path;
  }
  return "UNKNOWN";
}

int main(int argc, const char* argv[]) {
  @autoreleasepool {
    // Setup code that might create autoreleased objects goes here.
  }

  absl::InitializeSymbolizer(GetMainExecutablePath().c_str());
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

  return NSApplicationMain(argc, argv);
}
