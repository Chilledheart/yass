// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

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
#include <cassert>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "core/utils.hpp"
#include "core/utils_fs.hpp"

using json = nlohmann::json;
using namespace yass;

namespace config {

class ConfigImplLocal : public ConfigImpl {
 public:
  ConfigImplLocal(const std::string& path) : path_(ExpandUser(path)) {}
  ~ConfigImplLocal() override {}

 protected:
  bool OpenImpl(bool dontread) override {
    assert(!path_.empty() && "Opened with empty path");
    dontread_ = dontread;

    do {
      static constexpr const size_t kBufferSize = 32768;
      std::string buffer;
      buffer.resize(kBufferSize);

      ssize_t ret = ReadFileToBuffer(path_, absl::MakeSpan(buffer));
      if (ret <= 0) {
        std::cerr << "configure file failed to read: " << path_ << std::endl;
        break;
      }
      buffer.resize(ret);
      root_ = json::parse(buffer, nullptr, false);
      if (root_.is_discarded() || !root_.is_object()) {
        std::cerr << "bad config file: " << path_ << " content: \"" << buffer << "\"" << std::endl;
        break;
      }
      std::cerr << "loaded from config file: " << path_ << std::endl;
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
    if (!CreateDirectories(dir)) {
      std::cerr << "configure dir could not create: " << dir << std::endl;
      return false;
    }

    // Call with defaults except in the case of UTF-8 errors which we replace
    // invalid UTF-8 characters instead of throwing an exception.
    std::string json_content = root_.dump(4, ' ', false, nlohmann::detail::error_handler_t::replace);
    if (static_cast<ssize_t>(json_content.size()) != WriteFileWithBuffer(path_, json_content)) {
      std::cerr << "failed to write to path: \"" << path_ << " with content \"" << json_content << "\"" << std::endl;
      return false;
    }

    std::cerr << "written config file at " << path_ << std::endl;

    path_.clear();
    return true;
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
    if (root_.contains(key) && root_[key].is_string()) {
      *value = root_[key].get<std::string>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
    if (root_.contains(key) && root_[key].is_boolean()) {
      *value = root_[key].get<bool>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
    if (root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer()) {
      *value = root_[key].get<uint32_t>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int32_t>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    if (root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer()) {
      *value = root_[key].get<uint64_t>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int64_t>();
      return true;
    }
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool WriteImpl(const std::string& key, std::string_view value) override {
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
};

}  // namespace config

#endif  // H_CONFIG_CONFIG_IMPL_POSIX
