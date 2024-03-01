// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_SS_SERVER
#define H_SS_SERVER

#include "net/content_server.hpp"
#include "server/server_connection.hpp"

namespace server {

typedef net::ContentServer<ServerConnectionFactory> ServerServer;

}  // namespace server

#endif  // H_SS_SERVER
