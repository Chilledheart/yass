//
// socks5_request_parser.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "socks5_request_parser.hpp"

namespace socks5 {

method_select_request_parser::method_select_request_parser() { reset(); }

void method_select_request_parser::reset() { state_ = request_start; }

request_parser::request_parser() { reset(); }

void request_parser::reset() { state_ = request_start; }

} // namespace socks5
