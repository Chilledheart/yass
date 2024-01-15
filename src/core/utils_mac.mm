// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#ifdef __APPLE__

#include <absl/flags/internal/program_name.h>

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include <errno.h>
#include <locale.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include <base/strings/sys_string_conversions.h>
#include "core/logging.hpp"

bool SetCurrentThreadPriority(ThreadPriority priority) {
  // Changing the priority of the main thread causes performance regressions.
  // https://crbug.com/601270
  DCHECK(!pthread_main_np());
  qos_class_t qos_class = QOS_CLASS_UNSPECIFIED;
  int relative_priority = 0; // Undocumented

  switch (priority) {
    case ThreadPriority::BACKGROUND:
      qos_class = QOS_CLASS_BACKGROUND;
      relative_priority = 0;
      break;
    case ThreadPriority::NORMAL:
      qos_class = QOS_CLASS_UTILITY;
      relative_priority = 0;
      break;
    case ThreadPriority::ABOVE_NORMAL: {
      qos_class = QOS_CLASS_USER_INITIATED;
      relative_priority = 0;
      break;
    }
    case ThreadPriority::TIME_CRITICAL:
      qos_class = QOS_CLASS_USER_INTERACTIVE;
      relative_priority = 0;
      break;
  }
  /// documented in
  /// https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
  /// and https://developer.apple.com/library/archive/documentation/Performance/Conceptual/EnergyGuide-iOS/PrioritizeWorkWithQoS.html
  return pthread_set_qos_class_self_np(qos_class, relative_priority) == 0;

}

bool SetCurrentThreadName(const std::string& name) {
  // pthread_setname() fails (harmlessly) in the sandbox, ignore when it does.
  // See http://crbug.com/47058
  return pthread_setname_np(name.c_str()) == 0;
}

uint64_t GetMonotonicTime() {
  // https://www.manpagez.com/man/3/clock_gettime_nsec_np/
  // CLOCK_UPTIME_RAW is the same with mach_absolute_time();
  // but CLOCK_UPTIME_RAW does not increment while the system is asleep.
  //
  // use CLOCK_MONOTONIC_RAW here for continuing to increment while the system is asleep
  return clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
}

// TBD
bool IsProgramConsole() {
  return true;
}

bool SetUTF8Locale() {
  // C.UTF-8 doesn't exists on macOS
  if (setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
    return false;
  if (strcmp(setlocale(LC_ALL, nullptr), "en_US.UTF-8") != 0)
    return false;
  return true;
}

static std::string main_exe_path = "UNKNOWN";

bool GetExecutablePath(std::string* path) {
  char exe_path[PATH_MAX];
  uint32_t size = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &size) == 0) {
    char link_path[PATH_MAX];
    if (realpath(exe_path, link_path)) {
      *path = link_path;
      return true;
    } else {
      PLOG(WARNING) << "Internal error: realpath failed";
    }
  } else {
    PLOG(WARNING) << "Internal error: _NSGetExecutablePath failed";
  }
  *path = main_exe_path;
  return false;
}

void SetExecutablePath(const std::string& exe_path) {
  main_exe_path = exe_path;

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}

bool GetTempDir(std::string *path) {
  const char* env_tmpdir = getenv("TMPDIR");
  if (env_tmpdir) {
    *path = env_tmpdir;
    return true;
  }
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil) {
    return false;
  }
  *path = gurl_base::SysNSStringToUTF8(tmp);
  return true;
}

std::string GetHomeDir() {
  NSString* tmp = NSHomeDirectory();
  if (tmp != nil) {
    auto path = gurl_base::SysNSStringToUTF8(tmp);
    if (!path.empty()) {
      return path;
    }
  }
  // Fall back on temp dir if no home directory is defined.
  std::string rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
  return "/tmp";
}
#endif  // __APPLE__
