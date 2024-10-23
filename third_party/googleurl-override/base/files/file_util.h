// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILES_FILE_UTIL_H_
#define BASE_FILES_FILE_UTIL_H_

#include <string>

#include "base/base_export.h"
#include "build/build_config.h"

namespace gurl_base {

// Get the temporary directory provided by the system.
//
// WARNING: In general, you should use CreateTemporaryFile variants below
// instead of this function. Those variants will ensure that the proper
// permissions are set so that other users on the system can't edit them while
// they're open (which can lead to security issues).
BASE_EXPORT bool GetTempDir(std::string* path);
#if BUILDFLAG(IS_WIN)
BASE_EXPORT bool GetTempDir(std::wstring* path);
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
BASE_EXPORT void SetTempDir(const std::string& path);
#endif

// Get the home directory. This is more complicated than just getenv("HOME")
// as it knows to fall back on getpwent() etc.
//
// You should not generally call this directly. Instead use DIR_HOME with the
// path service which will use this function but cache the value.
// Path service may also override DIR_HOME.
BASE_EXPORT std::string GetHomeDir();
#if BUILDFLAG(IS_WIN)
BASE_EXPORT std::wstring GetHomeDirW();
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
BASE_EXPORT bool GetDataDir(std::string* path);
BASE_EXPORT void SetDataDir(const std::string& path);
#endif

}  // namespace gurl_base

#endif  // BASE_FILES_FILE_UTIL_H_
