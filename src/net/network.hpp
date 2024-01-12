// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020-2024 Chilledheart  */

#ifndef H_NET_NETWORK
#define H_NET_NETWORK

#ifdef _WIN32
#include <malloc.h>
#endif

#include "net/asio.hpp"

namespace net {

void SetSOReusePort(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code&);

void SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle,
                      asio::error_code&);

void SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code&);

void SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle,
                           asio::error_code&);

void SetTCPKeepAlive(asio::ip::tcp::acceptor::native_handle_type handle,
                     asio::error_code& ec);

void SetSocketLinger(asio::ip::tcp::socket* socket, asio::error_code&);

void SetSocketTcpNoDelay(asio::ip::tcp::socket* socket, asio::error_code& ec);

// from net/http/http_network_session.h
// Specifies the maximum HPACK dynamic table size the server is allowed to set.
const uint32_t kSpdyMaxHeaderTableSize = 64 * 1024;

// The maximum size of header list that the server is allowed to send.
const uint32_t kSpdyMaxHeaderListSize = 256 * 1024;

// Specifies the maximum concurrent streams server could send (via push).
const uint32_t kSpdyMaxConcurrentPushedStreams = 1000;

// Specifies the the default value for the push setting, which is disabled.
const uint32_t kSpdyDisablePush = 0;

// followed by curl's nghttp adapter
/* this is how much we want "in flight" for a stream */
#define H2_STREAM_WINDOW_SIZE   (10 * 1024 * 1024)
#define HTTP2_HUGE_WINDOW_SIZE (100 * H2_STREAM_WINDOW_SIZE)

// from net/spdy/spdy_session.h
// If more than this many bytes have been read or more than that many
// milliseconds have passed, return ERR_IO_PENDING from ReadLoop.
const int kYieldAfterBytesRead = 32 * 1024;
const int kYieldAfterDurationMilliseconds = 20;

// from net/spdy/spdy_session.h
// Maximum number of capped frames that can be queued at any time.
// We measured how many queued capped frames were ever in the
// SpdyWriteQueue at one given time between 2019-08 and 2020-02.
// The numbers showed that in 99.94% of cases it would always
// stay below 10, and that it would exceed 1000 only in
// 10^-8 of cases. Therefore we picked 10000 as a number that will
// virtually never be hit in practice, while still preventing an
// attacker from growing this queue unboundedly.
const int kSpdySessionMaxQueuedCappedFrames = 10000;

} // namespace net

#endif  // H_NET_NETWORK
