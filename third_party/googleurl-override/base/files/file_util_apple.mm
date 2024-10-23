// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <string.h>

#include "base/strings/sys_string_conversions.h"

namespace gurl_base {

bool GetTempDir(std::string* path) {
  // In order to facilitate hermetic runs on macOS, first check
  // MAC_CHROMIUM_TMPDIR. This is used instead of TMPDIR for historical reasons.
  // This was originally done for https://crbug.com/698759 (TMPDIR too long for
  // process singleton socket path), but is hopefully obsolete as of
  // https://crbug.com/1266817 (allows a longer process singleton socket path).
  // Continue tracking MAC_CHROMIUM_TMPDIR as that's what build infrastructure
  // sets on macOS.
  const char* env_tmpdir = getenv("GURL_BASE_TMPDIR");
  if (env_tmpdir) {
    *path = std::string(env_tmpdir);
    return true;
  }

  // If we didn't find it, fall back to the native function.
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil) {
    return false;
  }
  *path = gurl_base::SysNSStringToUTF8(tmp);
  return true;
}

std::string GetHomeDir() {
  NSString* tmp = NSHomeDirectory();
  if (tmp != nil) {
    auto path = SysNSStringToUTF8(tmp);
    if (!path.empty()) {
      return path;
    }
  }
  // Fall back on temp dir if no home directory is defined.
  std::string rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
  return "/tmp";
}

}  // namespace gurl_base
