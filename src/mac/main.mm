// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#import <Cocoa/Cocoa.h>

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
#include "third_party/boringssl/src/include/openssl/crypto.h"

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
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

  // argument values in the correct encoding and to make sure
  // generated file names (think downloads) are in the file system's
  // encoding.
  setlocale(LC_ALL, "");
  // For numbers we never want the C library's locale sensitive
  // conversion from number to string because the only thing it
  // changes is the decimal separator which is not good enough for
  // the UI and can be harmful elsewhere.
  setlocale(LC_NUMERIC, "C");

  absl::InitializeSymbolizer(exec_path.c_str());
#ifdef HAVE_CRASHPAD
  CHECK(InitializeCrashpad(exec_path));
#else
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);
#endif

  config::SetClientUsageMessage(exec_path);
  config::ReadConfigFileAndArguments(argc, argv);

  return NSApplicationMain(argc, argv);
}
