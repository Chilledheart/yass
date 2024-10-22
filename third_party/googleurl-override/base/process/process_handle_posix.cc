// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <unistd.h>

namespace gurl_base {

ProcessId GetCurrentProcId() {
  return getpid();
}

}  // namespace gurl_base
