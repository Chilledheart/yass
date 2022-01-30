// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/asio.hpp"

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
  return o << ec.message();
}
