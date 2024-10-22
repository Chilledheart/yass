// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/threading/platform_thread.h"

#include "base/dcheck_is_on.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/syscall.h>
#include <atomic>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
namespace internal {

void InvalidateTidCache();

} // namespace
#endif

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally.
thread_local pid_t g_thread_id = -1;

// A boolean value that indicates that the value stored in |g_thread_id| on the
// main thread is invalid, because it hasn't been updated since the process
// forked.
//
// This used to work by setting |g_thread_id| to -1 in a pthread_atfork handler.
// However, when a multithreaded process forks, it is only allowed to call
// async-signal-safe functions until it calls an exec() syscall. However,
// accessing TLS may allocate (see crbug.com/1275748), which is not
// async-signal-safe and therefore causes deadlocks, corruption, and crashes.
//
// It's Atomic to placate TSAN.
std::atomic<bool> g_main_thread_tid_cache_valid = false;

// Tracks whether the current thread is the main thread, and therefore whether
// |g_main_thread_tid_cache_valid| is relevant for the current thread. This is
// also updated by PlatformThread::CurrentId().
thread_local bool g_is_main_thread = true;

class InitAtFork {
 public:
  InitAtFork() {
    pthread_atfork(nullptr, nullptr, internal::InvalidateTidCache);
  }
};

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace internal {

void InvalidateTidCache() {
  g_main_thread_tid_cache_valid.store(false, std::memory_order_relaxed);
}

}  // namespace internal

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace gurl_base {

#if BUILDFLAG(IS_WIN)
// static
PlatformThreadId PlatformThread::CurrentId() {
  return ::GetCurrentThreadId();
}
#else
// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if BUILDFLAG(IS_APPLE)
  return pthread_mach_thread_np(pthread_self());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&g_thread_id, sizeof(pid_t));
  MSAN_UNPOISON(&g_is_main_thread, sizeof(bool));
  static InitAtFork init_at_fork;
  if (g_thread_id == -1 ||
      (g_is_main_thread &&
       !g_main_thread_tid_cache_valid.load(std::memory_order_relaxed))) {
    // Update the cached tid.
    g_thread_id = static_cast<pid_t>(syscall(__NR_gettid));
    // If this is the main thread, we can mark the tid_cache as valid.
    // Otherwise, stop the current thread from always entering this slow path.
    if (g_thread_id == getpid()) {
      g_main_thread_tid_cache_valid.store(true, std::memory_order_relaxed);
    } else {
      g_is_main_thread = false;
    }
  } else {
#if DCHECK_IS_ON()
    if (g_thread_id != syscall(__NR_gettid)) {
      RAW_LOG(
          FATAL,
          "Thread id stored in TLS is different from thread id returned by "
          "the system. It is likely that the process was forked without going "
          "through fork().");
    }
#endif
  }
  return g_thread_id;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif BUILDFLAG(IS_FUCHSIA)
  return zx_thread_self();
#elif BUILDFLAG(IS_SOLARIS) || BUILDFLAG(IS_QNX)
  return pthread_self();
#elif BUILDFLAG(IS_NACL) && defined(__GLIBC__)
  return pthread_self();
#elif BUILDFLAG(IS_NACL) && !defined(__GLIBC__)
  // Pointers are 32-bits in NaCl.
  return reinterpret_cast<int32_t>(pthread_self());
#elif BUILDFLAG(IS_POSIX) && BUILDFLAG(IS_AIX)
  return pthread_self();
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_AIX)
  return reinterpret_cast<int64_t>(pthread_self());
#endif
}
#endif
}  // namespace gurl_base
