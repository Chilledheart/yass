//
// completion_once_callback.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_COMPLETION_ONCE_CALLBACK
#define H_COMPLETION_ONCE_CALLBACK

#include <functional>

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
using CompletionOnceCallback = std::function<void(int)>;

#endif // H_COMPLETION_ONCE_CALLBACK
