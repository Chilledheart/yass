// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import <Cocoa/Cocoa.h>

#include <mach-o/dyld.h>
#include <stdexcept>
#include <string>
#include <locale.h>

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

#if defined(ARCH_CPU_X86_64)
// This is for https://crbug.com/1300598, and more generally,
// https://crbug.com/1297588 (and all of the associated bugs). It's horrible!
//
// When the main executable is updated on disk while the application is running,
// and the offset of the Mach-O image at the main executable's path changes from
// the offset that was determined when the executable was loaded, SecCode ceases
// to be able to work with the executable. This may be triggered when the
// product is updated on disk but the application has not yet relaunched. This
// affects SecCodeCopySelf and SecCodeCopyGuestWithAttributes. Bugs are evident
// even when validation (SecCodeCheckValidity) is not attempted.
//
__attribute__((used)) const char kGrossPaddingForCrbug1300598[68 * 1024] = {};
#endif

int main(int argc, const char* argv[]) {
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
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

  if (!MemoryLockAll()) {
    LOG(WARNING) << "Failed to set memory lock";
  }

  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;

  return NSApplicationMain(argc, argv);
}
