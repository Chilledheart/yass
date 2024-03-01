// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "net/socks4_request_parser.hpp"

namespace net {

namespace socks4 {

request_parser::request_parser() {
  reset();
}

void request_parser::reset() {
  state_ = request_start;
}

}  // namespace socks4

}  // namespace net
