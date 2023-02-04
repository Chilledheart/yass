// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CLI_SERVER
#define H_CLI_SERVER

#include "cli/cli_connection.hpp"
#include "content_server.hpp"

namespace cli {

typedef ContentServer<CliConnectionFactory> CliServer;

} // namespace cli

#endif  // H_CLI_SERVER
