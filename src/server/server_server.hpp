// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#ifndef H_SS_SERVER
#define H_SS_SERVER

#include "content_server.hpp"
#include "server_connection.hpp"

namespace server {

typedef ContentServer<ServerConnectionFactory> ServerServer;

} // namespace cli

#endif  // H_SS_SERVER
