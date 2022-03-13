// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CORE_ASIO
#define H_CORE_ASIO

#ifdef _WIN32
#include <malloc.h>  // for _alloca
#endif               // _WIN32

#include "core/iobuf.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#if defined(_MSC_VER) && !defined(__clang__)
#pragma push
// #pragma warning(pop): likely mismatch, popping warning state pushed in
// different file
#pragma warning(disable : 5031)
// pointer to potentially throwing function passed to extern C function
#pragma warning(disable : 5039)
#endif  // defined(_MSC_VER) && !defined(__clang__)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif  // defined(__clang__)

#include <asio.hpp>
#include "core/asio_throw_exceptions.hpp"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif  // defined(__clang__)

#pragma GCC diagnostic pop

#if defined(_MSC_VER) && !defined(__clang__)
#pragma pop
#endif  // defined(_MSC_VER) && !defined(__clang__)

extern std::ostream& operator<<(std::ostream& o, asio::error_code);

/// Create a new modifiable buffer that represents the given memory range.
/**
 * @returns <tt>mutable_buffer(tail, tailroom)</tt>.
 */
inline asio::ASIO_MUTABLE_BUFFER tail_buffer(IOBuf& io_buf) ASIO_NOEXCEPT
{
  return asio::ASIO_MUTABLE_BUFFER(io_buf.mutable_tail(), io_buf.tailroom());
}

/// Create a new modifiable buffer that represents the given memory range.
/**
 * @returns <tt>mutable_buffer(data, capacity)</tt>.
 */
inline asio::ASIO_MUTABLE_BUFFER mutable_buffer(IOBuf& io_buf) ASIO_NOEXCEPT
{
  return asio::ASIO_MUTABLE_BUFFER(io_buf.mutable_data(), io_buf.capacity());
}

/// Create a new non-modifiable buffer that represents the given memory range.
/**
 * @returns <tt>const_buffer(data, length)</tt>.
 */
inline asio::ASIO_CONST_BUFFER const_buffer(const IOBuf& io_buf) ASIO_NOEXCEPT
{
  return asio::ASIO_CONST_BUFFER(io_buf.data(), io_buf.length());
}

#endif  // H_CORE_ASIO
