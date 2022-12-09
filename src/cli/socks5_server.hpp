// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#ifndef H_SOCKS5_SERVER
#define H_SOCKS5_SERVER

#include "cli/socks5_connection.hpp"
#include "content_server.hpp"

typedef ContentServer<Socks5ConnectionFactory> Socks5Server;

#endif  // H_SOCKS5_SERVER
