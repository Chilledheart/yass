//
// config.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "config.hpp"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <gflags/gflags.h>
#include <json/json.h>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "cipher.hpp"

DEFINE_string(configfile, DEFAULT_CONFIGFILE, "load configs from file");
DEFINE_string(server_host, DEFAULT_SERVER,
              "IP address which remote server listens to");
DEFINE_int32(server_port, DEFAULT_SERVER_PORT,
             "Port number which remote server listens to");
DEFINE_string(password, DEFAULT_PASS, "Password pharsal");
DEFINE_string(method, DEFAULT_CIPHER, "Method of encrypt");
DEFINE_string(local_host, DEFAULT_LOCAL,
              "IP address which local server listens to");
DEFINE_int32(local_port, DEFAULT_LOCAL_PORT,
             "Port number which local server listens to");
DEFINE_bool(reuse_port, true, "Reuse the local port");

static std::string GetEnv(const char *name) {
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

static boost::filesystem::path ExpandUser(const std::string &file_path) {
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
static bool is_directory(const boost::filesystem::path &p) {
  DWORD dwAttrs = ::GetFileAttributesW(p.wstring().c_str());
  if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  return dwAttrs & FILE_ATTRIBUTE_DIRECTORY;
}

static bool create_directory(const boost::filesystem::path &p) {
  return ::CreateDirectoryW(p.wstring().c_str(), nullptr);
}
#else
static bool is_directory(const boost::filesystem::path &p) {
  struct stat Stat;
  std::string pStr = p.string();
  if (::stat(pStr.c_str(), &Stat) != 0) {
    return false;
  }
  return S_ISDIR(Stat.st_mode);
}

static bool create_directory(const boost::filesystem::path &p) {
  std::string pStr = p.string();
  return ::mkdir(pStr.c_str(), 0644) != 0;
}
#endif

static bool CreateConfigDirectory() {
  boost::filesystem::path real_path = ExpandUser(DEFAULT_CONFIGDIR);

  if (!is_directory(real_path) && !create_directory(real_path)) {
    return false;
  }
  return true;
}

bool ReadFromConfigfile(const std::string &file_path) {
  Json::Value root;
  boost::filesystem::ifstream fs;
  boost::filesystem::path real_path = ExpandUser(file_path);

  if (!CreateConfigDirectory()) {
    LOG(WARNING) << "configure dir does not exist: " << real_path;
    return false;
  }

  fs.open(real_path);

  if (!fs.is_open()) {
    LOG(WARNING) << "configure file does not exist: " << real_path;
    return false;
  }

  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  JSONCPP_STRING errs;
  if (!Json::parseFromStream(builder, fs, &root, &errs)) {
    LOG(WARNING) << "bad configuration: " << errs;
    return false;
  }

  const char *bad_field = nullptr;

  if (root.isMember("server") && root["server"].isString()) {
    FLAGS_server_host = root["server"].asString();
  } else {
    bad_field = "server";
  }

  if (root.isMember("server_port") && root["server_port"].isUInt()) {
    FLAGS_server_port = root["server_port"].asUInt();
  } else {
    bad_field = "server_port";
  }

  if (root.isMember("method") && root["method"].isString()) {
    FLAGS_method = root["method"].asString();
  } else {
    bad_field = "method";
  }

  if (root.isMember("password") && root["password"].isString()) {
    FLAGS_password = root["password"].asString();
  } else {
    bad_field = "password";
  }

  if (root.isMember("local") && root["local"].isString()) {
    FLAGS_local_host = root["local"].asString();
  } else {
    bad_field = "local";
  }

  if (root.isMember("local_port") && root["local_port"].isUInt()) {
    FLAGS_local_port = root["local_port"].asUInt();
  } else {
    bad_field = "local_port";
  }

  if (bad_field) {
    LOG(WARNING) << "bad field: " << bad_field << root[bad_field];
    return false;
  }

  cipher_method_in_use = to_cipher_method(FLAGS_method);

  return true;
}

bool SaveToConfigFile(const std::string &file_path) {
  Json::Value root;
  boost::filesystem::ofstream fs;
  boost::filesystem::path real_path = ExpandUser(file_path);

  if (!CreateConfigDirectory()) {
    LOG(WARNING) << "configure dir does not exist: " << real_path;
    return false;
  }

  fs.open(real_path);

  if (!fs.is_open()) {
    LOG(WARNING) << "configure file does not exist: " << real_path;
    return false;
  }

  FLAGS_method = to_cipher_method_str(cipher_method_in_use);

  root["server"] = FLAGS_server_host;
  root["server_port"] = FLAGS_server_port;
  root["method"] = FLAGS_method;
  root["password"] = FLAGS_password;
  root["local"] = FLAGS_local_host;
  root["local_port"] = FLAGS_local_port;

  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "   ";  // or whatever you like
  std::unique_ptr<Json::StreamWriter> writer(
      builder.newStreamWriter());
  if (writer->write(root, &fs) !=0) {
    return false;
  }

  return true;
}
