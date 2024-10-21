// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WARNING: You should *NOT* be using this class directly.  PlatformThread is
// the low-level platform-specific abstraction to the OS's threading interface.
// You should instead be using a message-loop driven Thread, see thread.h.

#ifndef BASE_THREADING_PLATFORM_THREAD_H_
#define BASE_THREADING_PLATFORM_THREAD_H_

#include <type_traits>

#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include <zircon/types.h>
#elif BUILDFLAG(IS_APPLE)
#include <mach/mach_types.h>
#elif BUILDFLAG(IS_POSIX)
#include <pthread.h>
#include <unistd.h>
#endif

namespace gurl_base {
// Used for logging. Always an integer value.
#if BUILDFLAG(IS_WIN)
typedef DWORD PlatformThreadId;
#elif BUILDFLAG(IS_FUCHSIA)
typedef zx_handle_t PlatformThreadId;
#elif BUILDFLAG(IS_APPLE)
typedef mach_port_t PlatformThreadId;
#elif BUILDFLAG(IS_POSIX)
typedef pid_t PlatformThreadId;
#endif
static_assert(std::is_integral_v<PlatformThreadId>, "Always an integer value.");

class BASE_EXPORT PlatformThread {
 public:
  // Gets the current thread id, which may be useful for logging purposes.
  static PlatformThreadId CurrentId();
};

}  // namespace gurl_base

#endif  // BASE_THREADING_PLATFORM_THREAD_H_
