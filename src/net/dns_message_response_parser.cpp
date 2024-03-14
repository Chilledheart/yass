// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#include "net/dns_message_response_parser.hpp"

namespace net::dns_message {

response_parser::response_parser() {
  reset();
}

void response_parser::reset() {
  state_ = response_start;
}

}  // namespace net::dns_message
