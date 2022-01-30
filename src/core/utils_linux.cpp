// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef __linux__
#include <time.h>
#include <pthread.h>

bool SetThreadPriority(std::thread::native_handle_type /*handle*/,
                       ThreadPriority /*priority*/) {
  // TBD: can be implemented with cgroup
  return true;
}

bool SetThreadName(std::thread::native_handle_type handle,
                   const std::string& name) {
  return pthread_setname_np(handle, name.c_str()) == 0;
}

uint64_t GetMonotonicTime() {
  static struct timespec start_ts;
  static bool started;
  struct timespec ts;
  int ret;
  if (!started) {
    ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
    if (ret < 0) {
      LOG(WARNING) << "clock_gettime failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (ret < 0) {
    LOG(WARNING) << "clock_gettime failed";
    return 0;
  }
  return static_cast<double>(ts.tv_sec - start_ts.tv_sec) * NS_PER_SECOND +
         ts.tv_nsec - start_ts.tv_nsec;
}
#endif
