// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "core/socks4_request_parser.hpp"

namespace socks4 {

request_parser::request_parser() {
  reset();
}

void request_parser::reset() {
  state_ = request_start;
}

}  // namespace socks4
