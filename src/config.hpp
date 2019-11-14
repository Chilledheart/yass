//
// config.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONFIG
#define H_CONFIG

#include <gflags/gflags_declare.h>

// TBD: read settinsg from json config file

#define DEFAULT_CONFIGDIR "~/.yass"
#define DEFAULT_CONFIGFILE "~/.yass/config.json"
#define DEFAULT_SERVER "0.0.0.0"
#define DEFAULT_SERVER_PORT 8443
#define DEFAULT_PASS "<default-pass>"
#define DEFAULT_CIPHER CRYPTO_PLAINTEXT_STR
#define DEFAULT_LOCAL "127.0.0.1"
#define DEFAULT_LOCAL_PORT 8000

DECLARE_string(configfile);
DECLARE_string(server_host);
DECLARE_int32(server_port);
DECLARE_string(password);
DECLARE_string(method);
DECLARE_string(local_host);
DECLARE_int32(local_port);
DECLARE_bool(reuse_port);
DECLARE_string(password);

// Not implemented
DECLARE_int32(timeout);
DECLARE_int32(fast_open);

void ReadFromConfigfile(const std::string &file_path);
void SaveToConfigFile(const std::string &file_path);

#endif // H_CONFIG
