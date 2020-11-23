// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "core/socks5_request_parser.hpp"

namespace socks5 {

method_select_request_parser::method_select_request_parser() { reset(); }

void method_select_request_parser::reset() { state_ = request_start; }

request_parser::request_parser() { reset(); }

void request_parser::reset() { state_ = request_start; }

} // namespace socks5
