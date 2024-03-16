// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#import <UIKit/UIKit.h>

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

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
#include "i18n/icu_util.hpp"
#include "ios/YassAppDelegate.h"
#include "version.h"

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

  absl::SetFlag(&FLAGS_v, 0);
  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_logtostderr, true);

#ifdef HAVE_ICU
  if (!InitializeICU()) {
    LOG(WARNING) << "Failed to initialize icu component";
  }
#endif

  NSString* appDelegateClassName;
  @autoreleasepool {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([YassAppDelegate class]);
  }
  absl::SetFlag(&FLAGS_logtostderr, false);
#ifdef NDEBUG
  absl::SetFlag(&FLAGS_stderrthreshold, LOGGING_WARNING);
#else
  absl::SetFlag(&FLAGS_stderrthreshold, LOGGING_VERBOSE);
#endif
  return UIApplicationMain(argc, (char**)argv, nil, appDelegateClassName);
}
