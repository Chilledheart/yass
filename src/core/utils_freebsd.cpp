// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include <build/build_config.h>

#include "core/logging.hpp"

#if BUILDFLAG(IS_FREEBSD)

#include <locale.h>
#include <pthread_np.h>
#include <time.h>  // For clock_gettime

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
      RAW_LOG(FATAL, "clock_gettime failed");
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
    RAW_LOG(FATAL, "clock_gettime failed");
    return 0;
  }
  ts.tv_sec -= start_ts.tv_sec;
  ts.tv_nsec -= start_ts.tv_nsec;
  return static_cast<uint64_t>(ts.tv_sec) * NS_PER_SECOND + ts.tv_nsec;
}

#endif  // BUILDFLAG(IS_FREEBSD)
