// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/asio.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
// new in GCC 7, see
// https://developers.redhat.com/blog/2017/03/10/wimplicit-fallthrough-in-gcc-7
#if (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif  // (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif  // defined(__clang__)

#include "asio/impl/src.hpp"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif  // defined(__clang__)

#pragma GCC diagnostic pop

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
  return o << ec.message();
}
