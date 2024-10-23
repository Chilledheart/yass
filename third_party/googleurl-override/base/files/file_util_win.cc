// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

struct IUnknown;
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"

namespace gurl_base {

bool GetTempDir(std::string* path) {
  std::wstring wpath;
  path->clear();
  if (!GetTempDir(&wpath)) {
    return false;
  }
  *path = SysWideToUTF8(wpath);
  return true;
}

bool GetTempDir(std::wstring* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPathW(MAX_PATH, temp_path);
  // If the function succeeds, the return value is the length,
  // in TCHARs, of the string copied to lpBuffer,
  // not including the terminating null character.
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  if (temp_path[path_len - 1] == L'\\') {
    temp_path[path_len - 1] = L'\0';
    --path_len;
  }
  *path = std::wstring(temp_path, path_len);
  DCHECK_NE((*path)[path_len - 1], L'\0');
  return true;
}

std::string GetHomeDir() {
  return SysWideToUTF8(GetHomeDirW());
}

std::wstring GetHomeDirW() {
  wchar_t result[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, result)) && result[0]) {
    return result;
  }
  // Fall back to the temporary directory on failure.
  std::wstring rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
  return L"C:\\";
}

}  // namespace gurl_base
