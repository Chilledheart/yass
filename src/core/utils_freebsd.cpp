// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef OS_FREEBSD

#include <locale.h>
#include <pthread_np.h>
#include <time.h> // For clock_gettime

// TBD
bool SetCurrentThreadPriority(ThreadPriority /*priority*/) {
  return true;
}

bool SetCurrentThreadName(const std::string& name) {
  pthread_set_name_np(pthread_self(), name.c_str());
  return true;
}

uint64_t GetMonotonicTime() {
  static struct timespec start_ts;
  static bool started;
  struct timespec ts;
  int ret;
  if (!started) {
    ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
    if (ret < 0) {
      PLOG(WARNING) << "clock_gettime failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
    PLOG(WARNING) << "clock_gettime failed";
    return 0;
  }
  return static_cast<double>(ts.tv_sec - start_ts.tv_sec) * NS_PER_SECOND +
         ts.tv_nsec - start_ts.tv_nsec;
}

// TBD
bool IsProgramConsole() {
  return true;
}

bool SetUTF8Locale() {
  if (setlocale(LC_ALL, "C.UTF-8") == nullptr)
    return false;
  if (strcmp(setlocale(LC_ALL, nullptr), "C.UTF-8") != 0)
    return false;
  return true;
}
#endif
