// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "net/ss_request_parser.hpp"

namespace net {

namespace ss {

request_parser::request_parser() {
  reset();
}

void request_parser::reset() {
  state_ = request_address_start;
}

}  // namespace ss

}  // namespace net
