// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "gui/utils.hpp"

#include "core/logging.hpp"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = ::getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

// returns true if the "file" exists and is a symbolic link
bool IsFile(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISREG(Stat.st_mode);
    }
  }
  return S_ISREG(Stat.st_mode);
}

// returns true if the "dir" exists and is a symbolic link
bool IsDirectory(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISDIR(Stat.st_mode);
    }
  }
  return S_ISDIR(Stat.st_mode);
}

bool CreatePrivateDirectory(const std::string& path) {
  return ::mkdir(path.c_str(), 0700) != 0;
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}

bool ReadFileToString(const std::string& path, std::string* context) {
  char buf[4096];
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = ::read(fd, buf, sizeof(buf) - 1);

  if (ret <= 0 || close(fd) < 0) {
    return false;
  }
  buf[ret] = '\0';
  *context = buf;
  return true;
}

bool WriteFileWithContent(const std::string& path, const std::string& context) {
  int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = ::write(fd, context.c_str(), context.size());

  if (ret != static_cast<long>(context.size()) || close(fd) < 0) {
    return false;
  }
  return true;
}

std::string GetAutostartDirectory() {
  const char* XdgConfigHome = getenv("XDG_CONFIG_HOME");
  if (!XdgConfigHome)
    XdgConfigHome = "~/.config";
  return ExpandUser(XdgConfigHome) + "/" + "autostart";
}
}  // namespace
bool Utils::GetAutoStart() {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  return IsFile(autostart_desktop);
}

void Utils::EnableAutoStart(bool on) {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  if (!on) {
    if (::unlink(autostart_desktop.c_str()) != 0) {
      LOG(WARNING) << "(Unexpected behavior): failed to unset autostart";
    }
  } else {
    // TODO: search XDG_DATA_DIRS instead of hard-coding which is like below
    // XDG_DATA_DIRS=/usr/share/gnome:/usr/local/share/:/usr/share/
    const char* origin_desktop =
        "/usr/share/applications/" DEFAULT_AUTOSTART_NAME ".desktop";
    std::string content;

    EnsureCreatedDirectory(GetAutostartDirectory());
    // force update, delete first
    if (IsFile(autostart_desktop)) {
      ::unlink(autostart_desktop.c_str());
    }

    // write to target
    if (!ReadFileToString(origin_desktop, &content) ||
        !WriteFileWithContent(autostart_desktop, content))  {
      LOG(WARNING) << "(Unexpected behavior): failed to set autostart";
    }
  }
}

