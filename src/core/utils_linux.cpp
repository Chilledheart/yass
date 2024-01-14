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
#include <sys/prctl.h>

bool SetCurrentThreadPriority(ThreadPriority /*priority*/) {
  // TBD: can be implemented with cgroup
  return true;
}

bool SetCurrentThreadName(const std::string& name) {
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (static_cast<pid_t>(pthread_self()) == getpid())
    return true;
  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
  int err = prctl(PR_SET_NAME, name.c_str());
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM)
    PLOG(ERROR) << "prctl(PR_SET_NAME)";
  return err == 0;
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
    ret = clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
    if (ret < 0) {
      PLOG(WARNING) << "clock_gettime failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed

  ret = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
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
