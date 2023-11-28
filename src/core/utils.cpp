// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "core/utils.hpp"

#include "config/config.hpp"

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif

#include <absl/flags/internal/program_name.h>

#ifdef HAVE_TCMALLOC
#include <tcmalloc/malloc_extension.h>
#endif

absl::StatusOr<int32_t> StringToInteger(const std::string& value) {
  long result = 0;
  char* endptr = nullptr;
  result = strtol(value.data(), &endptr, 10);
  if (result > INT32_MAX || (errno == ERANGE && result == LONG_MAX)) {
    return absl::InvalidArgumentError("overflow");
  } else if (result < INT_MIN || (errno == ERANGE && result == LONG_MIN)) {
    return absl::InvalidArgumentError("underflow");
  } else if (endptr - value.data() != static_cast<ptrdiff_t>(value.size())) {
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
      struct passwd pwd;
      struct passwd* result = nullptr;
      char buffer[PATH_MAX * 2] = {'\0'};
      uid_t uid = ::geteuid();
      int pwuid_res = ::getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &result);
      if (pwuid_res == 0 && result) {
        home = pwd.pw_dir;
      } else {
        home = "/";
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
      struct passwd pwd;
      struct passwd* result = nullptr;
      char buffer[PATH_MAX * 2] = {'\0'};
      auto pos = real_path.find("/", 1);
      auto username = real_path.substr(1, pos - 1);
      pos = real_path.find_first_not_of("/", pos);
      int pwnam_res = ::getpwnam_r(username.c_str(), &pwd, buffer, sizeof(buffer), &result);
      if (pwnam_res != 0 || result == nullptr) {
        return "/";
      }
      if (pos != std::string::npos) {
        return absl::StrCat(pwd.pw_dir, "/", real_path.substr(pos).c_str());
      } else {
        return pwd.pw_dir;
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

#if !defined(__APPLE__) && !defined(_WIN32)

static std::string main_exe_path = "UNKNOWN";

bool GetExecutablePath(std::string* path) {
  char exe_path[PATH_MAX];
  ssize_t ret = readlink("/proc/self/exe", exe_path, sizeof(exe_path));
  if (ret >= 0) {
    *path = std::string(exe_path, ret);
    return true;
  }
  *path = main_exe_path;
  return true;
}

void SetExecutablePath(const std::string& exe_path) {
  main_exe_path = exe_path;

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}
#endif

/*
 * Net_ipv6works() returns true if IPv6 seems to work.
 */
bool Net_ipv6works() {
  if (!absl::GetFlag(FLAGS_ipv6_mode)) {
    return false;
  }
#ifdef _WIN32
  using fd_t = SOCKET;
#else
  using fd_t = int;
#endif
  /* probe to see if we have a working IPv6 stack */
  fd_t s = socket(AF_INET6, SOCK_DGRAM, 0);
#ifndef _WIN32
  if (s < 0) {
#else
  if (s == INVALID_SOCKET) {
#endif
    return false;
  } else {
#ifndef _WIN32
    close(s);
#else
    closesocket(s);
#endif
    return true;
  }
}

#ifndef _WIN32
ssize_t ReadFileToBuffer(const std::string& path, char* buf, size_t buf_len) {
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return -1;
  }
  ssize_t ret = ::read(fd, buf, buf_len - 1);

  if (ret < 0 || close(fd) < 0) {
    return -1;
  }
  buf[ret] = '\0';
  return ret;
}

ssize_t WriteFileWithBuffer(const std::string& path,
                            const char* buf,
                            size_t buf_len) {
  int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = ::write(fd, buf, buf_len);

  if (ret < 0 || close(fd) < 0) {
    return -1;
  }
  return ret;
}
#endif

#ifdef HAVE_TCMALLOC
void PrintTcmallocStats() {
  std::vector<const char*> properties = {
    "generic.current_allocated_bytes",
    "generic.heap_size",
    "tcmalloc.max_total_thread_cache_bytes",
    "tcmalloc.current_total_thread_cache_bytes",
    "tcmalloc.pageheap_free_bytes",
    "tcmalloc.pageheap_unmapped_bytes",
  };
  for (auto property : properties) {
    absl::optional<size_t> size =
        tcmalloc::MallocExtension::GetNumericProperty(property);
    if (size.has_value()) {
      LOG(INFO) << "TCMALLOC: " << property << " = " << *size << " bytes";
    }
  }
}
#endif
