// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BASE_FILES_PLATFORM_FILE_H_
#define BASE_FILES_PLATFORM_FILE_H_
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#ifdef __MINGW32__
#include <windows.h>
#else
#include "base/win/windows_types.h"
#endif
#endif
// This file defines platform-independent types for dealing with
// platform-dependent files. If possible, use the higher-level base::File class
// rather than these primitives.
namespace gurl_base {
#if BUILDFLAG(IS_WIN)
using PlatformFile = HANDLE;
// It would be nice to make this constexpr but INVALID_HANDLE_VALUE is a
// ((void*)(-1)) which Clang rejects since reinterpret_cast is technically
// disallowed in constexpr. Visual Studio accepts this, however.
const PlatformFile kInvalidPlatformFile = INVALID_HANDLE_VALUE;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using PlatformFile = int;
constexpr PlatformFile kInvalidPlatformFile = -1;
#endif
}  // namespace gurl_base
#endif  // BASE_FILES_PLATFORM_FILE_H_
