//
// ss_factory.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SS_FACTORY
#define H_SS_FACTORY

#include "connection_factory.hpp"
#include "ss_connection.hpp"

typedef ServiceFactory<ss::SsConnection> SsFactory;

#endif // H_SS_FACTORY
