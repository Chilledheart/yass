// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "core/pr_util.hpp"

// TBD
PRThread *PR_GetCurrentThread(void) {
  static PRThread thread;
  return &thread;
}
