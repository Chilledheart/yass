// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef H_NET_ASIO
#define H_NET_ASIO

#ifdef _WIN32
#include <malloc.h>  // for _alloca
#endif               // _WIN32

#include <exception>
#include <thread>

#include "net/iobuf.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
#pragma push
// #pragma warning(pop): likely mismatch, popping warning state pushed in
// different file
#pragma warning(disable : 5031)
// pointer to potentially throwing function passed to extern C function
#pragma warning(disable : 5039)
#endif  // defined(_MSC_VER) && !defined(__clang__)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

#if 0
#define ASIO_ENABLE_HANDLER_TRACKING
#endif

#ifdef __MINGW32__
#undef _POSIX_THREADS
#endif
#include <asio.hpp>
#ifndef ASIO_NO_SSL
#include <asio/ssl.hpp>
#endif
#include "net/asio_throw_exceptions.hpp"

#pragma GCC diagnostic pop

#if defined(_MSC_VER) && !defined(__clang__)
#pragma pop
#endif  // defined(_MSC_VER) && !defined(__clang__)

extern std::ostream& operator<<(std::ostream& o, asio::error_code);

/// Create a new modifiable buffer that represents the given memory range.
/**
 * @returns <tt>mutable_buffer(tail, tailroom)</tt>.
 */
inline asio::ASIO_MUTABLE_BUFFER tail_buffer(net::IOBuf& io_buf, uint32_t max_length = UINT32_MAX) ASIO_NOEXCEPT {
  return asio::ASIO_MUTABLE_BUFFER(io_buf.mutable_tail(), std::min<uint32_t>(io_buf.tailroom(), max_length));
}

/// Create a new modifiable buffer that represents the given memory range.
/**
 * @returns <tt>mutable_buffer(data, capacity)</tt>.
 */
inline asio::ASIO_MUTABLE_BUFFER mutable_buffer(net::IOBuf& io_buf) ASIO_NOEXCEPT {
  return asio::ASIO_MUTABLE_BUFFER(io_buf.mutable_data(), io_buf.capacity());
}

/// Create a new non-modifiable buffer that represents the given memory range.
/**
 * @returns <tt>const_buffer(data, length)</tt>.
 */
inline asio::ASIO_CONST_BUFFER const_buffer(const net::IOBuf& io_buf) ASIO_NOEXCEPT {
  return asio::ASIO_CONST_BUFFER(io_buf.data(), io_buf.length());
}

#ifndef ASIO_NO_SSL
void load_ca_to_ssl_ctx(SSL_CTX* ssl_ctx);
#endif

#endif  // H_NET_ASIO
