// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_SS_SERVER
#define H_SS_SERVER

#include "net/content_server.hpp"
#include "server/server_connection.hpp"

namespace net::server {

using ServerConnectionFactory = ConnectionFactory<ServerConnection>;
using ServerServer = ContentServer<ServerConnectionFactory>;

}  // namespace net::server

#endif  // H_SS_SERVER
