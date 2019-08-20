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
#include <gflags/gflags.h>
#include <json/json.h>
#include <fstream>

DEFINE_string(configfile,   "", "load configs from file");
DEFINE_string(server_host, DEFAULT_SERVER, "IP address which remote server listens to");
DEFINE_int32(server_port,  DEFAULT_SERVER_PORT, "Port number which remote server listens to");
DEFINE_string(password,  DEFAULT_PASS, "Password pharsal");
DEFINE_string(method, DEFAULT_CIPHER, "Method of encrypt");
DEFINE_string(local_host,  DEFAULT_LOCAL, "IP address which local server listens to");
DEFINE_int32(local_port, DEFAULT_LOCAL_PORT, "Port number which local server listens to");
DEFINE_bool(reuse_port, true, "Reuse the local port");

void ReadFromConfigfile(const std::string &file_path) {
  Json::Value root;
  std::fstream fs;

  fs.open(file_path);

  fs >> root;
  if (root.isMember("server")) {
    FLAGS_server_host = root["server"].asString();
  }
  if (root.isMember("server_port")) {
    FLAGS_server_port = root["server_port"].asUInt();
  }
  if (root.isMember("method")) {
    FLAGS_method = root["method"].asString();
  }
  if (root.isMember("password")) {
    FLAGS_password = root["password"].asString();
  }
  if (root.isMember("local")) {
    FLAGS_local_host = root["local"].asString();
  }
  if (root.isMember("local_port")) {
    FLAGS_local_port = root["local_port"].asUInt();
  }
}
