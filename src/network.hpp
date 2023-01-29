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

#endif  // H_NETWORK
