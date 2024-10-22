// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"

#include <absl/time/clock.h>
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace gurl_base {
namespace debug {

static bool is_debug_ui_suppressed = false;

bool WaitForDebugger(int wait_seconds, bool silent) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  // The pid from which we know which process to attach to are not output by
  // android ddms, so we have to print it out explicitly.
  DLOG(INFO) << "DebugUtil::WaitForDebugger(pid=" << static_cast<int>(getpid())
             << ")";
#endif
  for (int i = 0; i < wait_seconds * 10; ++i) {
    if (BeingDebugged()) {
      if (!silent)
        BreakDebugger();
      return true;
    }
    // PlatformThread::Sleep(Milliseconds(100));
    absl::SleepFor(absl::Milliseconds(100));
  }
  return false;
}

void BreakDebugger() {
  BreakDebuggerAsyncSafe();
}

void SetSuppressDebugUI(bool suppress) {
  is_debug_ui_suppressed = suppress;
}

bool IsDebugUISuppressed() {
  return is_debug_ui_suppressed;
}

}  // namespace debug
}  // namespace gurl_base
