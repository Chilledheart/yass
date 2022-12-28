// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#include "core/logging.hpp"

#ifdef _WIN32

#include <windows.h>

ScopedClearLastError::ScopedClearLastError()
    : last_system_error_(GetLastError()) {
  SetLastError(0);
}

ScopedClearLastError::~ScopedClearLastError() {
  SetLastError(last_system_error_);
}

#endif  // _WIN32
