//
// config_impl_posix.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONFIG_CONFIG_IMPL_POSIX
#define H_CONFIG_CONFIG_IMPL_POSIX

#include "config/config_impl.hpp"

#ifndef _WIN32

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <json/json.h>
#include <memory>
#include <string>

#include "core/logging.hpp"

#define DEFAULT_CONFIGDIR "~/.yass"
#define DEFAULT_CONFIGFILE "~/.yass/config.json"

DEFINE_string(configfile, DEFAULT_CONFIGFILE, "load configs from file");


std::string ExpandUser(const std::string &file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

bool is_directory(const std::string &path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  return S_ISDIR(Stat.st_mode);
}

bool create_directory(const std::string &path) {
  return ::mkdir(path.c_str(), 0700) != 0;
}

bool CreateConfigDirectory(const std::string &configdir) {
  std::string real_path = ExpandUser(configdir);

  if (!is_directory(real_path) && !create_directory(real_path)) {
    return false;
  }
  return true;
}

bool ReadFile(const std::string &path, std::string* context) {
  char buf[4096];
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(WARNING) << "configure file does not exist: " << path;
    return false;
  }
  ssize_t ret = ::read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (ret <= 0) {
    LOG(WARNING) << "configure file failed to read: " << path;
    return false;
  }
  buf[ret] = '\0';
  *context = buf;
  return true;
}

bool WriteFile(const std::string &path, const std::string &context) {
  int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    LOG(WARNING) << "configure file does not exist: " << path;
    return false;
  }
  ssize_t ret = ::write(fd, context.c_str(), context.size()+1);
  close(fd);
  if (ret != static_cast<long>(context.size()) + 1) {
    LOG(WARNING) << "configure file failed to write: " << path;
    return false;
  }
  return true;
}

namespace config {

class ConfigImplPosix : public ConfigImpl {
public:
  ~ConfigImplPosix() override{};

  bool Open(bool dontread) override {
    dontread_ = dontread;
    if (!CreateConfigDirectory(DEFAULT_CONFIGDIR)) {
      LOG(WARNING) << "configure dir could not create: " << DEFAULT_CONFIGDIR;
      return false;
    }

    path_ = ExpandUser(FLAGS_configfile);

    if (!dontread) {
      std::string context;
      if (!ReadFile(path_, &context)) {
        return false;
      }
      Json::CharReaderBuilder builder;
      builder["collectComments"] = false;
      JSONCPP_STRING errs;
      const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

      if (!reader->parse(context.c_str(), context.c_str() + context.size() + 1, &root_, &errs)) {
        LOG(WARNING) << "bad configuration: " << errs;
        return false;
      }
    }

    return true;
  }

  bool Close() override {
    if (!dontread_) {
      return true;
    }
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "   "; // or whatever you like
    const std::string context = Json::writeString(builder, root_);
    if (!WriteFile(path_, context)){
      LOG(WARNING) << "failed to write: " << context;
      return false;
    }
    return true;
  }

  bool Read(const std::string &key, std::string *value) override {
    if (root_.isMember(key) && root_[key].isString()) {
      *value = root_[key].asString();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Read(const std::string &key, uint32_t *value) override {
    if (root_.isMember(key) && root_[key].isUInt()) {
      *value = root_[key].asUInt();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Read(const std::string &key, int32_t *value) override {
    if (root_.isMember(key) && root_[key].isInt()) {
      *value = root_[key].asInt();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Read(const std::string &key, uint64_t *value) override {
    if (root_.isMember(key) && root_[key].isUInt64()) {
      *value = root_[key].asUInt64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Read(const std::string &key, int64_t *value) override {
    if (root_.isMember(key) && root_[key].isInt64()) {
      *value = root_[key].asInt64();
      return true;
    }
    LOG(WARNING) << "bad field: " << key;
    return false;
  }

  bool Write(const std::string &key, const std::string &value) override {
    root_[key] = value;
    return true;
  }

  bool Write(const std::string &key, uint32_t value) override {
    root_[key] = value;
    return true;
  }

  bool Write(const std::string &key, int32_t value) override {
    root_[key] = value;
    return true;
  }

  bool Write(const std::string &key, uint64_t value) override {
    root_[key] = value;
    return true;
  }

  bool Write(const std::string &key, int64_t value) override {
    root_[key] = value;
    return true;
  }

private:
  std::string path_;
  Json::Value root_;
};

} // namespace config

#endif // _WIN32
#endif // H_CONFIG_CONFIG_IMPL_POSIX
