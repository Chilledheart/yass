// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#include "core/utils.hpp"
#include "core/utils_fs.hpp"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x601
#include <filesystem>
#else
#include <windows.h>
#endif

namespace yass {

#if !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x601

// returns true if the "file" exists and is a symbolic link
bool IsFile(const std::string& path) {
  std::error_code ec;
  auto stat = std::filesystem::status(path, ec);
  if (ec || !std::filesystem::is_regular_file(stat)) {
    return false;
  }
  return true;
}

// returns true if the "dir" exists and is a symbolic link
bool IsDirectory(const std::string& path) {
  if (path == "." || path == "..") {
    return true;
  }
  std::error_code ec;
  auto stat = std::filesystem::status(path, ec);
  if (ec || !std::filesystem::is_directory(stat)) {
    return false;
  }
  return true;
}

bool CreateDir(const std::string& path) {
  std::error_code ec;
  std::filesystem::create_directory(path, ec);
  if (ec) {
    return false;
  }
  return true;
}

bool CreateDirectories(const std::string& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    return false;
  }
  return true;
}

bool RemoveFile(const std::string& path) {
  std::error_code ec;
  std::filesystem::path p(path);
  std::filesystem::remove(p, ec);
  if (ec) {
    return false;
  }
  return true;
}

#else

bool IsFile(const std::string& path) {
  std::wstring wpath = SysUTF8ToWide(path);
  DWORD dwAttrib = ::GetFileAttributesW(wpath.c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsDirectory(const std::string& path) {
  if (path == "." || path == "..") {
    return true;
  }
  std::wstring wpath = SysUTF8ToWide(path);
  DWORD dwAttrib = ::GetFileAttributesW(wpath.c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool CreateDir(const std::string& path) {
  std::wstring wpath = SysUTF8ToWide(path);
  return ::CreateDirectoryW(wpath.c_str(), nullptr);
}

bool CreateDirectories(const std::string& path) {
  if (!IsDirectory(path) && !CreateDir(path)) {
    return false;
  }
  return true;
}

bool RemoveFile(const std::string& path) {
  std::wstring wpath = SysUTF8ToWide(path);
  if (::DeleteFileW(wpath.c_str()) == FALSE) {
    return false;
  }
  return true;
}

#endif

} // yass namespace

