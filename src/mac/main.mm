// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import <Cocoa/Cocoa.h>

#include <locale.h>
#include <mach-o/dyld.h>
#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_cat.h>
#include <openssl/crypto.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
#include "i18n/icu_util.hpp"
#include "version.h"

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

int main(int argc, const char** argv) {
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }

  absl::InitializeSymbolizer(exec_path.c_str());
#ifdef HAVE_CRASHPAD
  CHECK(InitializeCrashpad(exec_path));
#else
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);
#endif

  absl::SetProgramUsageMessage(absl::StrCat(
      "Usage: ", Basename(exec_path), " [options ...]\n", " -K, --config <file> Read config from a file\n",
      " --server_host <host> Remote server on given host\n", " --server_port <port> Remote server on given port\n",
      " --local_host <host> Local proxy server on given host\n"
      " --local_port <port> Local proxy server on given port\n"
      " --username <username> Server user\n",
      " --password <pasword> Server password\n", " --method <method> Specify encrypt of method to use"));

  config::ReadConfigFileOption(argc, argv);
  config::ReadConfig();
  absl::ParseCommandLine(argc, const_cast<char**>(argv));

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
  }
#endif

  CRYPTO_library_init();

  return NSApplicationMain(argc, argv);
}
