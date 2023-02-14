// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020-2023 Chilledheart  */

#ifndef H_NETWORK
#define H_NETWORK

#ifdef _WIN32
#include <malloc.h>
#endif

#include <absl/flags/declare.h>

#include "core/asio.hpp"

ABSL_DECLARE_FLAG(bool, reuse_port);
ABSL_DECLARE_FLAG(std::string, congestion_algorithm);
ABSL_DECLARE_FLAG(bool, tcp_fastopen);
ABSL_DECLARE_FLAG(bool, tcp_fastopen_connect);
ABSL_DECLARE_FLAG(int32_t, connect_timeout);
ABSL_DECLARE_FLAG(int32_t, tcp_connection_timeout);
ABSL_DECLARE_FLAG(int32_t, tcp_user_timeout);
ABSL_DECLARE_FLAG(int32_t, so_linger_timeout);
ABSL_DECLARE_FLAG(int32_t, so_snd_buffer);
ABSL_DECLARE_FLAG(int32_t, so_rcv_buffer);

ABSL_DECLARE_FLAG(bool, tcp_keep_alive);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_cnt);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_idle_timeout);
ABSL_DECLARE_FLAG(int32_t, tcp_keep_alive_interval);
ABSL_DECLARE_FLAG(bool, tls13_early_return);
ABSL_DECLARE_FLAG(bool, redir_mode);

void SetSOReusePort(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code&);

void SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle,
                      asio::error_code&);

void SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code&);

void SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle,
                           asio::error_code&);

void SetTCPConnectionTimeout(asio::ip::tcp::socket::native_handle_type handle,
                             asio::error_code&);

void SetTCPUserTimeout(asio::ip::tcp::socket::native_handle_type handle,
                       asio::error_code&);

void SetTCPKeepAlive(asio::ip::tcp::acceptor::native_handle_type handle,
                     asio::error_code& ec);

void SetSocketLinger(asio::ip::tcp::socket* socket, asio::error_code&);

void SetSocketSndBuffer(asio::ip::tcp::socket* socket, asio::error_code&);

void SetSocketRcvBuffer(asio::ip::tcp::socket* socket, asio::error_code&);

ABSL_DECLARE_FLAG(bool, padding_support);

constexpr int kFirstPaddings = 8;
constexpr int kPaddingHeaderSize = 3;
constexpr int kMaxPaddingSize = 255;

void AddPadding(std::shared_ptr<IOBuf> buf);
std::shared_ptr<IOBuf> RemovePadding(std::shared_ptr<IOBuf> buf, asio::error_code &ec);

// from net/http/http_network_session.h
// Specifies the maximum HPACK dynamic table size the server is allowed to set.
const uint32_t kSpdyMaxHeaderTableSize = 64 * 1024;

// The maximum size of header list that the server is allowed to send.
const uint32_t kSpdyMaxHeaderListSize = 256 * 1024;

// Specifies the maximum concurrent streams server could send (via push).
const uint32_t kSpdyMaxConcurrentPushedStreams = 1000;

// Specifies the the default value for the push setting, which is disabled.
const uint32_t kSpdyDisablePush = 0;

// from net/http/http_network_session.cc
// The maximum receive window sizes for HTTP/2 sessions and streams.
const int32_t kSpdySessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32_t kSpdyStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB

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

#endif  // H_NETWORK
