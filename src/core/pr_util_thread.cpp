//
// pr_util_thread.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "core/pr_util.hpp"

// TBD
PRThread *PR_GetCurrentThread(void) {
  static PRThread thread;
  return &thread;
}
