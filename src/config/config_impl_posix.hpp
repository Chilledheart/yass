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

#include <sys/stat.h>
#include <sys/types.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <glog/logging.h>
#include <json/json.h>
#include <memory>
#include <string>

#define DEFAULT_CONFIGDIR "~/.yass"
#define DEFAULT_CONFIGFILE "~/.yass/config.json"

DEFINE_string(configfile, DEFAULT_CONFIGFILE, "load configs from file");

inline std::string GetEnv(const char *name) {
#ifdef _WIN32
  char buf[4096];
  size_t len = 0;
  if (getenv_s(&len, buf, name) != 0 || len <= 1) {
    return std::string();
  }

  return std::string(buf, len - 1);
#else
  return getenv(name);
#endif
}

inline boost::filesystem::path ExpandUser(const std::string &file_path) {
  std::string real_path = file_path;

  if (real_path.size() >= 1 && real_path[0] == '~') {
#ifdef _WIN32
    std::string home = GetEnv("USERPROFILE");
#else
    std::string home = GetEnv("HOME");
#endif
    return boost::filesystem::path(home) / real_path.substr(2);
  }

  return boost::filesystem::path(real_path);
}

#ifdef _WIN32
inline bool is_directory(const boost::filesystem::path &p) {
  DWORD dwAttrs = ::GetFileAttributesW(p.wstring().c_str());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  return dwAttrs & FILE_ATTRIBUTE_DIRECTORY;
}

inline bool create_directory(const boost::filesystem::path &p) {
  return ::CreateDirectoryW(p.wstring().c_str(), nullptr);
}
#else
inline bool is_directory(const boost::filesystem::path &p) {
  struct stat Stat;
  std::string pStr = p.string();
  if (::stat(pStr.c_str(), &Stat) != 0) {
    return false;
  }
  return S_ISDIR(Stat.st_mode);
}

inline bool create_directory(const boost::filesystem::path &p) {
  std::string pStr = p.string();
  return ::mkdir(pStr.c_str(), 0700) != 0;
}
#endif

inline bool CreateConfigDirectory(const std::string &configdir) {
  boost::filesystem::path real_path = ExpandUser(configdir);

  if (!is_directory(real_path) && !create_directory(real_path)) {
    return false;
  }
  return true;
}

namespace config {

class ConfigImplPosix : public ConfigImpl {
public:
  ~ConfigImplPosix() override{};

  bool Open(bool dontread) override {
    if (!CreateConfigDirectory(DEFAULT_CONFIGDIR)) {
      LOG(WARNING) << "configure dir does not exist: " << DEFAULT_CONFIGDIR;
      return false;
    }

    if (!dontread) {
      boost::filesystem::path real_path = ExpandUser(FLAGS_configfile);
      fs_.open(real_path);
      if (!fs_.is_open()) {
        LOG(WARNING) << "configure file does not exist: " << real_path;
        return false;
      }

      Json::CharReaderBuilder builder;
      builder["collectComments"] = false;
      JSONCPP_STRING errs;

      if (!Json::parseFromStream(builder, fs_, &root_, &errs)) {
        LOG(WARNING) << "bad configuration: " << errs;
        return false;
      }
    }

    return true;
  }

  bool Close() override {
    fs_.seekp(0, std::ios_base::beg);

    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "   "; // or whatever you like
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    if (writer->write(root_, &fs_) != 0) {
      return false;
    }

    fs_.flush();
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
  Json::Value root_;
  boost::filesystem::fstream fs_;
};

} // namespace config

#endif // _WIN32
#endif // H_CONFIG_CONFIG_IMPL_POSIX
