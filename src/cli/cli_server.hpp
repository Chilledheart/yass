// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CLI_SERVER
#define H_CLI_SERVER

#include "cli/cli_connection.hpp"
#include "net/content_server.hpp"

namespace cli {

typedef net::ContentServer<CliConnectionFactory> CliServer;

}  // namespace cli

#endif  // H_CLI_SERVER
