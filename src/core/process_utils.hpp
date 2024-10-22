// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef CORE_PROCESS_UTIL
#define CORE_PROCESS_UTIL

#include <stdint.h>
#include <string>
#include <vector>

#ifndef _WIN32
int ExecuteProcess(const std::vector<std::string>& params, std::string* output, std::string* error);
#endif

#endif  // CORE_PROCESS_UTIL
