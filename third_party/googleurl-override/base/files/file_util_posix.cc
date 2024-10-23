// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace gurl_base {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
static std::string g_cache_dir;
void SetTempDir(const std::string& path) {
  g_cache_dir = path;
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
static std::string g_data_dir;
bool GetDataDir(std::string* path) {
  if (g_data_dir.empty()) {
    return false;
  }
  *path = g_data_dir;
  return true;
}
void SetDataDir(const std::string& path) {
  g_data_dir = path;
}
#endif

#if !BUILDFLAG(IS_APPLE)
// This is implemented in file_util_apple.mm for Mac.
bool GetTempDir(std::string* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp) {
    *path = tmp;
    return true;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  // return PathService::Get(DIR_CACHE, path);
  if (!g_cache_dir.empty()) {
    *path = g_cache_dir;
    return true;
  }
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  *path = "/data/local/tmp";
#else
  *path = "/tmp";
#endif
  return true;
}
#endif  // !BUILDFLAG(IS_APPLE)

#if !BUILDFLAG(IS_APPLE)  // Mac implementation is in file_util_apple.mm.
std::string GetHomeDir() {
  const char* home_dir = getenv("HOME");
  if (home_dir && home_dir[0]) {
    return home_dir;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  RAW_LOG(ERROR, "OS_ANDROID: Home directory lookup not yet implemented.");
#endif
  // Fall back on temp dir if no home directory is defined.
  std::string rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS)
  return "/data/local/tmp";
#else
  return "/tmp";
#endif
}
#endif  // !BUILDFLAG(IS_APPLE)

}  // namespace gurl_base
