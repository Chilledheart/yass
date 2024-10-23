// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef H_CRASHPAD_HELPER_H
#define H_CRASHPAD_HELPER_H

#include <filesystem>
#include <string>

#ifdef HAVE_CRASHPAD
bool InitializeCrashpad(const std::string& exe_path,
                        const std::string& temp_path = std::filesystem::temp_directory_path().string());
#endif

#endif  // H_CRASHPAD_HELPER_H
