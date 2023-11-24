// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef CORE_PROCESS_UTIL
#define CORE_PROCESS_UTIL

#include <vector>
#include <string>
#include <stdint.h>

#ifndef _WIN32
int ExecuteProcess(const std::vector<std::string>& params,
                   std::string* output,
                   std::string* error);
#endif

#ifdef _MSC_VER
// On Windows, process id and thread id are of the same type according to the
// return types of GetProcessId() and GetThreadId() are both DWORD, an unsigned
// 32-bit type.
using pid_t = uint32_t;
#else
#include <unistd.h>
#endif

pid_t GetPID();
pid_t GetTID();
pid_t GetMainThreadPid();
bool PidHasChanged();

#endif // CORE_PROCESS_UTIL
