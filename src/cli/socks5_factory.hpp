//
// socks5_factory.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SOCKS5_FACTORY
#define H_SOCKS5_FACTORY

#include "cli/socks5_connection.hpp"
#include "connection_factory.hpp"

typedef ServiceFactory<socks5::Socks5Connection> Socks5Factory;

#endif // H_SOCKS5_FACTORY
