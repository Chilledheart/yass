// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_POSIX
#define H_CONFIG_CONFIG_IMPL_POSIX

#include "config/config_impl.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <absl/flags/flag.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "core/logging.hpp"
#include "core/utils.hpp"

using json = nlohmann::json;

namespace {

#ifdef _WIN32
bool IsDirectory(const std::string& path) {
  if (path == "." || path == "..") {
    return true;
  }
  DWORD dwAttrib = GetFileAttributesA(path.c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool CreatePrivateDirectory(const std::string& path) {
  return CreateDirectoryA(path.c_str(), nullptr);
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}
#else
bool IsDirectory(const std::string& path) {
  if (path == "." || path == "..") {
    return true;
  }
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
#endif
}  // anonymous namespace

namespace config {

class ConfigImplLocal : public ConfigImpl {
 public:
  ConfigImplLocal(const std::string &path) : path_(ExpandUser(path)) {}
  ~ConfigImplLocal() override{}

 protected:
  bool OpenImpl(bool dontread) override {
    DCHECK(!path_.empty());
    dontread_ = dontread;

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

    std::string json_content = root_.dump(4);
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

#endif  // H_CONFIG_CONFIG_IMPL_POSIX
