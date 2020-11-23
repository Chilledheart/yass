//
// pr_error.cpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/pr_util.hpp"
#include <stdlib.h>
#include <string.h>

PRErrorCode PR_GetError(void) {
  PRThread *thread = PR_GetCurrentThread();
  return thread->errorCode;
}

int32_t PR_GetOSError(void) {
  PRThread *thread = PR_GetCurrentThread();
  return thread->osErrorCode;
}

void PR_SetError(PRErrorCode code, int32_t osErr) {
  PRThread *thread = PR_GetCurrentThread();
  thread->errorCode = code;
  thread->osErrorCode = osErr;
  thread->errorStringLength = 0;
}

void PR_SetErrorText(int textLength, const char *text) {
  PRThread *thread = PR_GetCurrentThread();

  if (0 == textLength) {
    if (NULL != thread->errorString) {
      delete (thread->errorString);
    }
    thread->errorStringSize = 0;
  } else {
    int size =
        textLength + 31; /* actual length to allocate. Plus a little extra */
    if (thread->errorStringSize < textLength + 1) /* do we have room? */
    {
      if (NULL != thread->errorString) {
        delete (thread->errorString);
      }
      thread->errorString = new char[size];
      if (NULL == thread->errorString) {
        thread->errorStringSize = 0;
        thread->errorStringLength = 0;
        return;
      }
      thread->errorStringSize = size;
    }
    memcpy(thread->errorString, text, textLength + 1);
  }
  thread->errorStringLength = textLength;
}

int32_t PR_GetErrorTextLength(void) {
  PRThread *thread = PR_GetCurrentThread();
  return thread->errorStringLength;
} /* PR_GetErrorTextLength */

int32_t PR_GetErrorText(char *text) {
  PRThread *thread = PR_GetCurrentThread();
  if (0 != thread->errorStringLength)
    memcpy(text, thread->errorString, thread->errorStringLength + 1);
  return thread->errorStringLength;
} /* PR_GetErrorText */
