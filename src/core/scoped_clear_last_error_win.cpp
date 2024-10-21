// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#include "core/logging.hpp"

#ifdef _WIN32

#include <windows.h>

namespace gurl_base {
namespace logging {

ScopedClearLastError::ScopedClearLastError() : last_system_error_(GetLastError()) {
  SetLastError(0);
}

ScopedClearLastError::~ScopedClearLastError() {
  SetLastError(last_system_error_);
}

}  // namespace logging
}  // namespace gurl_base

#endif  // _WIN32
