// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/pr_log.hpp"
#ifdef WIN32
#include <windows.h>
#endif
#ifdef ANDROID
#include <android/log.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void PR_Abort(void) {
  // PR_LogPrint("Aborting");
#ifdef ANDROID
  __android_log_write(ANDROID_LOG_ERROR, "PRLog", "Aborting");
#endif
  abort();
}

void PR_Assert(const char *s, const char *file, int ln) {
  // PR_LogPrint("Assertion failure: %s, at %s:%d\n", s, file, ln);
  fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
  fflush(stderr);
#ifdef WIN32
  DebugBreak();
#elif defined(XP_OS2)
  asm("int $3");
#elif defined(ANDROID)
  __android_log_assert(NULL, "PRLog", "Assertion failure: %s, at %s:%d\n", s,
                       file, ln);
#endif
  abort();
}
