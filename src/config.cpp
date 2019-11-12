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
#include "cipher.hpp"
#include <boost/filesystem.hpp>
#include <gflags/gflags.h>
#include <json/json.h>

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

using namespace boost::filesystem;

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

static path ExpandUser(const std::string &file_path) {
  std::string real_path = file_path;

  if (real_path.size() >= 1 && real_path[0] == '~') {
#ifdef _WIN32
    std::string home = GetEnv("USERPROFILE");
#else
    std::string home = GetEnv("HOME");
#endif
    return path(home) / real_path.substr(2);
  }

  return path(real_path);
}

static void CreateConfigDirectory() {
  path real_path = ExpandUser(DEFAULT_CONFIGDIR);
  boost::system::error_code ec = boost::system::error_code();
  if (!is_directory(real_path, ec)) {
    create_directories(real_path, ec);
  }
}

void ReadFromConfigfile(const std::string &file_path) {
  Json::Value root;
  boost::filesystem::ifstream fs;
  path real_path = ExpandUser(file_path);

  CreateConfigDirectory();

  fs.open(real_path);

  if (!fs.is_open()) {
    LOG(WARNING) << "configure file does not exist: " << real_path;
    return;
  }

  try {
    fs >> root;
    if (root.isMember("server") && root["server"].isString()) {
      FLAGS_server_host = root["server"].asString();
    }
    if (root.isMember("server_port") && root["server_port"].isUInt()) {
      FLAGS_server_port = root["server_port"].asUInt();
    }
    if (root.isMember("method") && root["method"].isString()) {
      FLAGS_method = root["method"].asString();
    }
    if (root.isMember("password") && root["password"].isString()) {
      FLAGS_password = root["password"].asString();
    }
    if (root.isMember("local") && root["local"].isString()) {
      FLAGS_local_host = root["local"].asString();
    }
    if (root.isMember("local_port") && root["local_port"].isUInt()) {
      FLAGS_local_port = root["local_port"].asUInt();
    }
  } catch (std::exception &e) {
    LOG(WARNING) << "bad configuration: " << e.what();
  }

  cipher_method = to_cipher_method(FLAGS_method);
}

void SaveToConfigFile(const std::string &file_path) {
  Json::Value root;
  boost::filesystem::ofstream fs;
  path real_path = ExpandUser(file_path);

  CreateConfigDirectory();

  fs.open(real_path);

  FLAGS_method = to_cipher_method_str(cipher_method);

  root["server"] = FLAGS_server_host;
  root["server_port"] = FLAGS_server_port;
  root["method"] = FLAGS_method;
  root["password"] = FLAGS_password;
  root["local"] = FLAGS_local_host;
  root["local_port"] = FLAGS_local_port;

  fs << root;
}
