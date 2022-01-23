// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONFIG_CONFIG_IMPL_POSIX
#define H_CONFIG_CONFIG_IMPL_POSIX

#include "config/config_impl.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Define before rapidjson
// Optimization is defaulted by gcc/clang
#ifdef __SSE2__
#define RAPIDJSON_SSE2 1
#endif

#ifdef __SSE4_2__
#define RAPIDJSON_SSE42 1
#endif

#ifdef __ARM_NEON
#define RAPIDJSON_NEON 1
#endif

#define RAPIDJSON_HAS_STDSTRING 1

#include <absl/flags/flag.h>
#include <rapidjson/document.h>     // rapidjson's DOM-style API
#include <rapidjson/prettywriter.h> // for stringify JSON
#include <memory>
#include <string>

#include "core/logging.hpp"
#include "core/utils.hpp"

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
  ~ConfigImplPosix() override{};

 protected:
  bool OpenImpl(bool dontread) override {
    dontread_ = dontread;

    path_ = ExpandUser(absl::GetFlag(FLAGS_configfile));

    if (!dontread) {
      ssize_t size =
          ReadFileToBuffer(path_, read_buffer_, sizeof(read_buffer_));
      if (size < 0) {
        LOG(WARNING) << "configure file failed to read: " << path_;
        return false;
      }
      if (root_.ParseInsitu(read_buffer_).HasParseError() ||
          !root_.IsObject()) {
        LOG(WARNING) << "bad configure file: " << root_.GetParseError()
                     << " content: \"" << read_buffer_ << "\"";
        return false;
      }
      VLOG(2) << "loaded from config file " << path_;
    } else {
      root_.SetObject();
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

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    if (!root_.Accept(writer)) {
      LOG(WARNING) << "invalid json object";
      return false;
    }
    if (static_cast<ssize_t>(sb.GetSize()) !=
        WriteFileWithBuffer(path_, sb.GetString(), sb.GetSize())) {
      LOG(WARNING) << "failed to write to path: \"" << path_
                   << " with content \"" << sb.GetString();
      return false;
    }

    VLOG(2) << "written from config file " << path_;

    path_.clear();
    return true;
  }

  bool ReadImpl(const std::string& key, std::string* value) override {
    if (root_.HasMember(key) && root_[key].IsString()) {
      *value = root_[key].GetString();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, bool* value) override {
    if (root_.HasMember(key) && root_[key].IsBool()) {
      *value = root_[key].GetBool();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint32_t* value) override {
    if (root_.HasMember(key) && root_[key].IsUint()) {
      *value = root_[key].GetUint();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int32_t* value) override {
    if (root_.HasMember(key) && root_[key].IsInt()) {
      *value = root_[key].GetInt();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, uint64_t* value) override {
    if (root_.HasMember(key) && root_[key].GetUint64()) {
      *value = root_[key].GetUint64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool ReadImpl(const std::string& key, int64_t* value) override {
    if (root_.HasMember(key) && root_[key].GetInt64()) {
      *value = root_[key].GetInt64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool WriteImpl(const std::string& key, absl::string_view value) override {
    if (root_.HasMember(key))
      root_[key].SetString(value.data(), value.size());
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      // TODO why do we still make a copy if we use string_view
      rapidjson::Value v(value.data(), value.size(), root_.GetAllocator());
      root_.AddMember(k, v, root_.GetAllocator());
    }
    return true;
  }

  bool WriteImpl(const std::string& key, bool value) override {
    if (root_.HasMember(key))
      root_[key].SetBool(value);
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      root_.AddMember(k, value, root_.GetAllocator());
    }
    return true;
  }

  bool WriteImpl(const std::string& key, uint32_t value) override {
    if (root_.HasMember(key))
      root_[key].SetUint(value);
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      root_.AddMember(k, value, root_.GetAllocator());
    }
    return true;
  }

  bool WriteImpl(const std::string& key, int32_t value) override {
    if (root_.HasMember(key))
      root_[key].SetInt(value);
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      root_.AddMember(k, value, root_.GetAllocator());
    }
    return true;
  }

  bool WriteImpl(const std::string& key, uint64_t value) override {
    if (root_.HasMember(key))
      root_[key].SetUint64(value);
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      root_.AddMember(k, value, root_.GetAllocator());
    }
    return true;
  }

  bool WriteImpl(const std::string& key, int64_t value) override {
    if (root_.HasMember(key))
      root_[key].SetInt64(value);
    else {
      rapidjson::Value k(key, root_.GetAllocator());
      root_.AddMember(k, value, root_.GetAllocator());
    }
    return true;
  }

 private:
  std::string path_;
  rapidjson::Document root_;
  char read_buffer_[4096];
};

}  // namespace config

#endif  // !defined(_WIN32) && !defined(__APPLE__)
#endif  // H_CONFIG_CONFIG_IMPL_POSIX
