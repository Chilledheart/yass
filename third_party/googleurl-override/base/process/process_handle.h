// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_PROCESS_HANDLE_H_
#define BASE_PROCESS_PROCESS_HANDLE_H_

#include <stdint.h>
#include <sys/types.h>

#include <compare>
#include <iosfwd>

#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace gurl_base {

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
// ProcessId is a number which identifies the process in the OS.
#if BUILDFLAG(IS_WIN)
typedef HANDLE ProcessHandle;
typedef DWORD ProcessId;
typedef HANDLE UserTokenHandle;
const ProcessHandle kNullProcessHandle = NULL;
const ProcessId kNullProcessId = 0;
#elif BUILDFLAG(IS_FUCHSIA)
typedef zx_handle_t ProcessHandle;
typedef zx_koid_t ProcessId;
const ProcessHandle kNullProcessHandle = ZX_HANDLE_INVALID;
const ProcessId kNullProcessId = ZX_KOID_INVALID;
#elif BUILDFLAG(IS_POSIX)
// On POSIX, our ProcessHandle will just be the PID.
typedef pid_t ProcessHandle;
typedef pid_t ProcessId;
const ProcessHandle kNullProcessHandle = 0;
const ProcessId kNullProcessId = 0;
#endif  // BUILDFLAG(IS_WIN)

// Returns the id of the current process.
// Note that on some platforms, this is not guaranteed to be unique across
// processes (use GetUniqueIdForProcess if uniqueness is required).
BASE_EXPORT ProcessId GetCurrentProcId();

}  // namespace gurl_base

#endif  // BASE_PROCESS_PROCESS_HANDLE_H_
