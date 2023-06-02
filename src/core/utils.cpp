// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

absl::StatusOr<int32_t> StringToInteger(absl::string_view value) {
  long result = 0;
  char* endptr = nullptr;
  result = strtol(value.data(), &endptr, 10);
  if (result > INT32_MAX || (errno == ERANGE && result == LONG_MAX)) {
    return absl::InvalidArgumentError("overflow");
  } else if (result < INT_MIN || (errno == ERANGE && result == LONG_MIN)) {
    return absl::InvalidArgumentError("underflow");
  } else if (*endptr != '\0') {
    return absl::InvalidArgumentError("bad integer");
  }
  return static_cast<int32_t>(result);
}

#ifdef _WIN32
const char kSeparators[] = "/\\";
#else
const char kSeparators[] = "/";
#endif

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = ::getenv("HOME");
    if (home.empty()) {
#ifdef _WIN32
      home = absl::StrCat(::getenv("HOMEDRIVE"), "\\", ::getenv("HOMEPATH"));
#else
      struct passwd *pw = getpwuid(geteuid());
      if (pw) {
        home = pw->pw_dir;
      }
#endif
    }
    if (real_path.size() == 1) {
      return home;
    }
    // ~username
    if (real_path[1] != '/') {
#ifdef _WIN32
      return absl::StrCat(::getenv("HOMEDRIVE"), "\\Users");
#else
      struct passwd *pw = nullptr;
      auto pos = real_path.find("/", 1);
      auto username = real_path.substr(1, pos - 1);
      pos = real_path.find_first_not_of("/", pos);
      pw = ::getpwnam(username.c_str());
      if (pw == nullptr) {
        return std::string();
      }
      if (pos != std::string::npos) {
        return absl::StrCat(pw->pw_dir, "/", real_path.substr(pos).c_str());
      } else {
        return pw->pw_dir;
      }
#endif
    }
    // ~/path/to/directory
    return absl::StrCat(home,
#ifdef _WIN32
                        "\\",
#else
                        "/",
#endif
                        real_path.substr(2));
  }

  return real_path;
}
