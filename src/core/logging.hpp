// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef H_LOGGING
#define H_LOGGING

#include <system_error>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <glog/logging.h>
#define NOTREACHED() LOG(FATAL)

#endif // H_LOGGING
