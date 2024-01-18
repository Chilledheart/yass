// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#include "core/utils.hpp"
#include "core/utils_fs.hpp"

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
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
#endif

#ifndef _WIN32
bool RemoveFile(const std::string& path) {
  const char* path_str = path.c_str();
  struct stat file_info {};
  if (::lstat(path_str, &file_info) != 0) {
    // The Windows version defines this condition as success.
    return (errno == ENOENT);
  }
  if (S_ISDIR(file_info.st_mode)) {
    return (rmdir(path_str) == 0) || (errno == ENOENT);
  } else {
    return (unlink(path_str) == 0) || (errno == ENOENT);
  }
}
#else
// Returns the Win32 last error code or ERROR_SUCCESS if the last error code is
// ERROR_FILE_NOT_FOUND or ERROR_PATH_NOT_FOUND. This is useful in cases where
// the absence of a file or path is a success condition (e.g., when attempting
// to delete an item in the filesystem).
static DWORD ReturnLastErrorOrSuccessOnNotFound() {
  const DWORD error_code = ::GetLastError();
  return (error_code == ERROR_FILE_NOT_FOUND ||
          error_code == ERROR_PATH_NOT_FOUND)
             ? ERROR_SUCCESS
             : error_code;
}

bool RemoveFile(const std::string& path) {
  std::wstring wpath = SysUTF8ToWide(path);
  const DWORD attr = ::GetFileAttributesW(wpath.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES)
    return ReturnLastErrorOrSuccessOnNotFound() == ERROR_SUCCESS;
  // Clear the read-only bit if it is set.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !::SetFileAttributesW(wpath.c_str(),
                            attr & ~DWORD{FILE_ATTRIBUTE_READONLY})) {
    // It's possible for |path| to be gone now under a race with other deleters.
    return ReturnLastErrorOrSuccessOnNotFound() == ERROR_SUCCESS;
  }
  if (attr & FILE_ATTRIBUTE_DIRECTORY) {
    return ::RemoveDirectoryW(wpath.c_str()) == TRUE ? true :
      ReturnLastErrorOrSuccessOnNotFound() == ERROR_SUCCESS;
  } else {
    return ::DeleteFileW(wpath.c_str()) == TRUE ? true :
      ReturnLastErrorOrSuccessOnNotFound() == ERROR_SUCCESS;
  }
}
#endif

} // yass namespace

