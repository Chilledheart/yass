// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef H_COMPLETION_ONCE_CALLBACK
#define H_COMPLETION_ONCE_CALLBACK

#include <functional>

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
using CompletionOnceCallback = std::function<void(int)>;

#endif  // H_COMPLETION_ONCE_CALLBACK
