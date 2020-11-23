// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "ss_request_parser.hpp"

namespace ss {

request_parser::request_parser() { reset(); }

void request_parser::reset() { state_ = request_address_start; }

} // namespace ss
