// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef __linux__
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <syscall.h>  // For syscall.
#include <sys/mman.h> // For mlockall.
#include <sys/time.h>
#include <sys/resource.h>

bool SetThreadPriority(std::thread::native_handle_type /*handle*/,
                       ThreadPriority /*priority*/) {
  // TBD: can be implemented with cgroup
  return true;
}

bool SetThreadName(std::thread::native_handle_type handle,
                   const std::string& name) {
  if (handle == 0) {
    handle = pthread_self();
  }
  return pthread_setname_np(handle, name.c_str()) == 0;
}

bool MemoryLockAll() {
  if (mlockall(MCL_CURRENT) == 0)
    return true;
  PLOG(WARNING) << "Failed to call mlockall";
  struct rlimit rlim;
  if (errno == ENOMEM && ::getrlimit(RLIMIT_MEMLOCK, &rlim) == 0) {
    LOG(WARNING) << "Please Increase RLIMIT_MEMLOCK, soft limit: "
      << rlim.rlim_cur << ", hard limit: " << rlim.rlim_max;
  }
  return false;
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
