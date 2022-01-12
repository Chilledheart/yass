// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_POSIX
#define H_CONFIG_CONFIG_IMPL_POSIX

#include "config/config_impl.hpp"

#ifndef _WIN32

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <absl/flags/flag.h>
#include <json/json.h>
#include <memory>
#include <string>

#include "core/logging.hpp"

ABSL_FLAG(std::string,
          configfile,
          "~/.yass/config.json",
          "load configs from file");

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

// A portable interface that returns the basename of the filename passed as an
// argument. It is similar to dirname(3)
// <https://linux.die.net/man/3/dirname>.
// For example:
//     Dirname("a/b/prog/file.cc")
// returns "a/b/prog"
//     Dirname("file.cc")
// returns "."
//     Dirname("/file.cc")
// returns "/"
inline absl::string_view Dirname(absl::string_view filename) {
  auto last_slash_pos = filename.find_last_of("/\\");

  return last_slash_pos == absl::string_view::npos
             ? "."
             : filename.substr(0, last_slash_pos);
}

bool IsDirectory(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
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
  ssize_t ret = ::write(fd, context.c_str(), context.size() + 1);

  if (ret != static_cast<long>(context.size()) + 1 || close(fd) < 0) {
    return false;
  }
  return true;
}

}  // anonymous namespace

namespace config {

class ConfigImplPosix : public ConfigImpl {
 public:
  ~ConfigImplPosix() override{};

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;

    path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

    if (!dontread) {
      std::string content;
      if (!ReadFileToString(path_, &content)) {
        LOG(WARNING) << "configure file failed to read: " << path_;
        return false;
      }
      Json::CharReaderBuilder builder;
      builder["collectComments"] = false;
      JSONCPP_STRING errs;
      const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

      if (!reader->parse(content.c_str(), content.c_str() + content.size() + 1,
                         &root_, &errs)) {
        LOG(WARNING) << "bad configuration: " << errs << " content: \""
                     << content << "\"";
        return false;
      }
      VLOG(2) << "read from config file " << path_;
    }

    return true;
  }

  bool CloseImpl() override {
    if (path_.empty() || !dontread_) {
      return true;
    }

    auto dir_ref = Dirname(path_);
    std::string dir(dir_ref.data(), dir_ref.size());
    if (!EnsureCreatedDirectory(dir)) {
      LOG(WARNING) << "configure dir could not create: " << dir;
      return false;
    }

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "   ";  // or whatever you like
    const std::string content = Json::writeString(builder, root_);
    if (!WriteFileWithContent(path_, content)) {
      LOG(WARNING) << "failed to write to path: \"" << path_
                   << " with content \"" << content << "\"";
      return false;
    }

    VLOG(2) << "written from config file " << path_;

    path_.clear();
    return true;
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
    if (root_.isMember(key) && root_[key].isString()) {
      *value = root_[key].asString();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
    if (root_.isMember(key) && root_[key].isBool()) {
      *value = root_[key].asBool();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
    if (root_.isMember(key) && root_[key].isUInt()) {
      *value = root_[key].asUInt();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    if (root_.isMember(key) && root_[key].isInt()) {
      *value = root_[key].asInt();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    if (root_.isMember(key) && root_[key].isUInt64()) {
      *value = root_[key].asUInt64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    if (root_.isMember(key) && root_[key].isInt64()) {
      *value = root_[key].asInt64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool WriteImpl(const std::string& key, absl::string_view value) override {
    root_[key] = value.data();
    return true;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    root_[key] = static_cast<bool>(value);
    return true;
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    root_[key] = static_cast<Json::UInt>(value);
    return true;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    root_[key] = static_cast<Json::Int>(value);
    return true;
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    root_[key] = static_cast<Json::UInt64>(value);
    return true;
  }

  bool WriteImpl(const std::string& key, int64_t value) override {
    root_[key] = static_cast<Json::Int64>(value);
    return true;
  }

 private:
  std::string path_;
  Json::Value root_;
};

}  // namespace config

#endif  // _WIN32
#endif  // H_CONFIG_CONFIG_IMPL_POSIX
