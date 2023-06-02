// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_POSIX
#define H_CONFIG_CONFIG_IMPL_POSIX

#include "config/config_impl.hpp"

#if !defined(_WIN32)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <absl/flags/flag.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "core/logging.hpp"
#include "core/utils.hpp"

ABSL_FLAG(std::string,
          configfile,
          "~/.yass/config.json",
          "load configs from file");

using json = nlohmann::json;

namespace {

bool IsDirectory(const std::string& path) {
  struct stat Stat {};
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  return S_ISDIR(Stat.st_mode);
}

bool CreatePrivateDirectory(const std::string& path) {
  return ::mkdir(path.c_str(), 0700) == 0;
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}

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

}  // anonymous namespace

namespace config {

class ConfigImplPosix : public ConfigImpl {
 public:
  ~ConfigImplPosix() override{}

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;

    path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

    do {
      ssize_t size =
          ReadFileToBuffer(path_, read_buffer_, sizeof(read_buffer_));
      if (size < 0) {
        LOG(WARNING) << "configure file failed to read: " << path_;
        break;
      }
      root_ = json::parse(read_buffer_, nullptr, false);
      if (root_.is_discarded() || !root_.is_object()) {
        LOG(WARNING) << "bad config file: " << path_
                     << " content: \"" << read_buffer_ << "\"";
        break;
      }
      VLOG(2) << "loaded from config file: " << path_;
      return true;
    } while (false);

    if (!dontread) {
      return false;
    }

    root_ = json::object();

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

    std::string json_content = root_.dump();
    if (static_cast<ssize_t>(json_content.size()) !=
        WriteFileWithBuffer(path_, json_content.c_str(), json_content.size())) {
      LOG(WARNING) << "failed to write to path: \"" << path_
                   << " with content \"" << json_content;
      return false;
    }

    VLOG(2) << "written from config file " << path_;

    path_.clear();
    return true;
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
    if (root_.contains(key) && root_[key].is_string()) {
      *value = root_[key].get<std::string>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
    if (root_.contains(key) && root_[key].is_boolean()) {
      *value = root_[key].get<bool>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
    if (root_.contains(key) && root_[key].is_number_unsigned() &&
        root_[key].is_number_integer()) {
      *value = root_[key].get<uint32_t>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int32_t>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    if (root_.contains(key) && root_[key].is_number_unsigned() &&
        root_[key].is_number_integer()) {
      *value = root_[key].get<uint64_t>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int64_t>();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool WriteImpl(const std::string& key, absl::string_view value) override {
    root_[key] = std::string(value.data(), value.size());
    return true;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    root_[key] = value;
    return true;
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    root_[key] = value;
    return true;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    root_[key] = value;
    return true;
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    root_[key] = value;
    return true;
  }

  bool WriteImpl(const std::string& key, int64_t value) override {
    root_[key] = value;
    return true;
  }

  bool DeleteImpl(const std::string& key) override {
    auto iter = root_.find(key);
    if (iter != root_.end()) {
      root_.erase(iter);
      return true;
    }
    return false;
  }

 private:
  std::string path_;
  json root_;
  char read_buffer_[4096];
};

}  // namespace config

#endif  // !defined(_WIN32)
#endif  // H_CONFIG_CONFIG_IMPL_POSIX
