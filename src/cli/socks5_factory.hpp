// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_SOCKS5_FACTORY
#define H_SOCKS5_FACTORY

#include "cli/socks5_connection.hpp"
#include "connection_factory.hpp"

typedef ServiceFactory<socks5::Socks5Connection> Socks5Factory;

#endif // H_SOCKS5_FACTORY
