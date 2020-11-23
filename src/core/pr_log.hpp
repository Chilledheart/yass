// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef PR_LOG
#define PR_LOG

#include <assert.h>
#include <stdint.h>

void PR_Abort(void);
void PR_Assert(const char *s, const char *file, int ln);

#endif // PR_LOG
