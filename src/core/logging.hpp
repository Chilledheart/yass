//
// logging.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_LOGGING
#define H_LOGGING

#include <system_error>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <glog/logging.h>
#define NOTREACHED() LOG(FATAL)

#endif // H_LOGGING
