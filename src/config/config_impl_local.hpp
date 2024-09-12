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
#include <string>
#include <string_view>
#include "core/utils.hpp"
#include "core/utils_fs.hpp"

#ifdef HAVE_JSONCPP
#include <json/json.h>
#else
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

using namespace yass;

static constexpr const size_t kReadBufferSize = 32768u;

namespace config {

class ConfigImplLocal : public ConfigImpl {
 public:
  ConfigImplLocal(std::string_view path) : path_(ExpandUser(path)) {}
  ~ConfigImplLocal() override {}

 protected:
  bool OpenImpl(bool dontread) override {
    assert(!path_.empty() && "Opened with empty path");
    dontread_ = dontread;

    do {
      char buffer[kReadBufferSize];

      ssize_t ret = ReadFileToBuffer(path_, as_writable_bytes(make_span(buffer)));
      if (ret <= 0) {
        std::cerr << "configure file failed to read: " << path_ << std::endl;
        break;
      }
      if (ret == static_cast<ssize_t>(sizeof(buffer))) {
        std::cerr << "configure file is too large: " << path_ << std::endl;
        break;
      }
      std::string_view json_content(buffer, ret);
#ifdef HAVE_JSONCPP
      Json::CharReaderBuilder builder;
      builder["collectComments"] = false;
      Json::String err;
      const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
      if (!reader->parse(json_content.data(), json_content.data() + json_content.size(), &root_, &err)) {
        std::cerr << "bad config file: " << path_ << " err: " << err << " content: \"" << json_content << "\""
                  << std::endl;
        break;
      }
      if (!root_.isObject()) {
        std::cerr << "bad config file: " << path_ << " content: \"" << json_content << "\"" << std::endl;
        break;
      }
#else
      root_ = json::parse(json_content, nullptr, false);
      if (root_.is_discarded() || !root_.is_object()) {
        std::cerr << "bad config file: " << path_ << " content: \"" << json_content << "\"" << std::endl;
        break;
      }
#endif
      std::cerr << "loaded from config file: " << path_ << std::endl;
      return true;
    } while (false);

    if (!dontread) {
      return false;
    }

#ifdef HAVE_JSONCPP
    root_ = Json::objectValue;
#else
    root_ = json::object();
#endif

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
#ifdef HAVE_JSONCPP
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "    ";
    const std::string json_content = Json::writeString(builder, root_);
#else
    // Call with defaults except in the case of UTF-8 errors which we replace
    // invalid UTF-8 characters instead of throwing an exception.
    const std::string json_content = root_.dump(4, ' ', false, nlohmann::detail::error_handler_t::replace);
#endif
    if (static_cast<ssize_t>(json_content.size()) != WriteFileWithBuffer(path_, json_content)) {
      std::cerr << "failed to write to path: \"" << path_ << " with content \"" << json_content << "\"" << std::endl;
      return false;
    }

    std::cerr << "written config file at " << path_ << std::endl;

    path_.clear();
    return true;
  }

  bool HasKeyStringImpl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isString();
#else
    return root_.contains(key) && root_[key].is_string();
#endif
  }

  bool HasKeyBoolImpl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isBool();
#else
    return root_.contains(key) && root_[key].is_boolean();
#endif
  }

  bool HasKeyUint32Impl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isUInt();
#else
    return root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer();
#endif
  }

  bool HasKeyUint64Impl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isUInt64();
#else
    return root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer();
#endif
  }

  bool HasKeyInt32Impl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isInt();
#else
    return root_.contains(key) && root_[key].is_number_integer();
#endif
  }

  bool HasKeyInt64Impl(const std::string& key) override {
#ifdef HAVE_JSONCPP
    return root_.isMember(key) && root_[key].isInt64();
#else
    return root_.contains(key) && root_[key].is_number_integer();
#endif
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isString()) {
      *value = root_[key].asString();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_string()) {
      *value = root_[key].get<std::string>();
      return true;
    }
#endif
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isBool()) {
      *value = root_[key].asBool();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_boolean()) {
      *value = root_[key].get<bool>();
      return true;
    }
#endif
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isUInt()) {
      *value = root_[key].asUInt();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer()) {
      *value = root_[key].get<uint32_t>();
      return true;
    }
#endif
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isInt()) {
      *value = root_[key].asInt();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int32_t>();
      return true;
    }
#endif
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isUInt64()) {
      *value = root_[key].asUInt64();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_number_unsigned() && root_[key].is_number_integer()) {
      *value = root_[key].get<uint64_t>();
      return true;
    }
#endif
    std::cerr << "bad field: " << key << std::endl;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
#ifdef HAVE_JSONCPP
    if (root_.isMember(key) && root_[key].isInt64()) {
      *value = root_[key].asInt64();
      return true;
    }
#else
    if (root_.contains(key) && root_[key].is_number_integer()) {
      *value = root_[key].get<int64_t>();
      return true;
    }
#endif
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
#ifdef HAVE_JSONCPP
    Json::Value got;
    return root_.removeMember(key, &got);
#else
    auto iter = root_.find(key);
    if (iter != root_.end()) {
      root_.erase(iter);
      return true;
    }
    return false;
#endif
  }

 private:
  std::string path_;
#ifdef HAVE_JSONCPP
  Json::Value root_;
#else
  json root_;
#endif
};

}  // namespace config

#endif  // H_CONFIG_CONFIG_IMPL_POSIX
