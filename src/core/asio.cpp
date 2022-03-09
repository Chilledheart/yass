// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/asio.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#pragma push
// #pragma warning(pop): likely mismatch, popping warning state pushed in
// different file
#pragma warning(disable : 5031)
// pointer to potentially throwing function passed to extern C function
#pragma warning(disable : 5039)
#endif  // defined(_MSC_VER) && !defined(__clang__)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
// new in GCC 7, see
// https://developers.redhat.com/blog/2017/03/10/wimplicit-fallthrough-in-gcc-7
#if (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif  // (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)

#include "asio/impl/src.hpp"

#pragma GCC diagnostic pop

#if defined(_MSC_VER) && !defined(__clang__)
#pragma pop
#endif  // defined(_MSC_VER) && !defined(__clang__)

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
  return o << ec.message();
}
