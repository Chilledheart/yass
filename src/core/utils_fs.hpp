// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef YASS_FS_UTILS
#define YASS_FS_UTILS
#include <string>

namespace yass {

bool IsDirectory(const std::string& path);
bool IsFile(const std::string& path);
bool CreateDir(const std::string& path);
bool CreateDirectories(const std::string& path);
bool RemoveFile(const std::string& path);

} // yass namespace
#endif // YASS_FS_UTILS
