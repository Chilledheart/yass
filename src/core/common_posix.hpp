// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_COMMON_POSIX_H
#define CORE_COMMON_POSIX_H

#include "core/compiler_specific.hpp"

#if defined(OS_POSIX)

#include <errno.h>

#if defined(NDEBUG)

#define HANDLE_EINTR(x)                                     \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#else  // NDEBUG

#define HANDLE_EINTR(x)                                      \
  ({                                                         \
    int eintr_wrapper_counter = 0;                           \
    decltype(x) eintr_wrapper_result;                        \
    do {                                                     \
      eintr_wrapper_result = (x);                            \
    } while (eintr_wrapper_result == -1 && errno == EINTR && \
             eintr_wrapper_counter++ < 100);                 \
    eintr_wrapper_result;                                    \
  })

#endif  // NDEBUG

#endif  //  defined(OS_POSIX)

#endif  // CORE_COMMON_POSIX_H
