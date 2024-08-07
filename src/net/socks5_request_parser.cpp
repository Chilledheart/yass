// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "net/socks5_request_parser.hpp"

namespace net::socks5 {

method_select_request_parser::method_select_request_parser() {
  reset();
}

void method_select_request_parser::reset() {
  state_ = request_start;
}

auth_request_parser::auth_request_parser() {
  reset();
}

void auth_request_parser::reset() {
  state_ = request_start;
}

request_parser::request_parser() {
  reset();
}

void request_parser::reset() {
  state_ = request_start;
}

}  // namespace net::socks5
