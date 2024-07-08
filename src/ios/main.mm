// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#import <UIKit/UIKit.h>

#include <locale.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <signal.h>
#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
#include "ios/YassAppDelegate.h"
#include "version.h"

namespace config {
const ProgramType pType = YASS_CLIENT_GUI;
}  // namespace config

int main(int argc, const char** argv) {
  // setup signal handler
  signal(SIGPIPE, SIG_IGN);

  /* Block SIGPIPE in all threads, this can happen if a thread calls write on
     a closed pipe. */
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask failed");
    return -1;
  }
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  absl::InitializeSymbolizer(exec_path.c_str());
#ifdef HAVE_CRASHPAD
  CHECK(InitializeCrashpad(exec_path));
#else
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);
#endif

  config::SetClientUsageMessage(exec_path);
  config::ReadConfigFileAndArguments(argc, argv);

  NSString* appDelegateClassName;
  @autoreleasepool {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([YassAppDelegate class]);
  }

  return UIApplicationMain(argc, (char**)argv, nil, appDelegateClassName);
}
