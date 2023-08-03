// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef CORE_PROCESS_UTIL
#define CORE_PROCESS_UTIL

#include <vector>
#include <string>

int ExecuteProcess(const std::vector<std::string>& params,
                   std::string* output,
                   std::string* error);

#endif // CORE_PROCESS_UTIL
