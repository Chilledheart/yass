// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CLI_SERVER
#define H_CLI_SERVER

#include "cli/cli_connection.hpp"
#include "net/content_server.hpp"

namespace net::cli {

using CliConnectionFactory = ConnectionFactory<CliConnection>;
using CliServer = ContentServer<CliConnectionFactory>;

}  // namespace net::cli

#endif  // H_CLI_SERVER
