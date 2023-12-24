// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import <UIKit/UIKit.h>

#include <mach-o/dyld.h>
#include <stdexcept>
#include <string>
#include <locale.h>

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
#include "crypto/crypter_export.hpp"
#include "version.h"
#include "crashpad_helper.hpp"
#include "i18n/icu_util.hpp"
#include "ios/YassAppDelegate.h"

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

  absl::SetFlag(&FLAGS_v, 0);
  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_logtostderr, true);

#if 0
  if (!MemoryLockAll()) {
    LOG(WARNING) << "Failed to set memory lock";
  }
#endif

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
  }
#endif

  CRYPTO_library_init();

  NSString * appDelegateClassName;
  @autoreleasepool {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([YassAppDelegate class]);
  }
  return UIApplicationMain(argc, (char**)argv, nil, appDelegateClassName);
}
