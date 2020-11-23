//
// pr_log.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef PR_LOG
#define PR_LOG

#include <assert.h>
#include <stdint.h>

void PR_Abort(void);
void PR_Assert(const char *s, const char *file, int ln);

#endif // PR_LOG