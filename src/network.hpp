// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Chilledheart  */

#ifndef H_NETWORK
#define H_NETWORK

#ifdef _WIN32
#include <malloc.h>
#endif

#include <asio/error.hpp>
#include <asio/ip/tcp.hpp>

asio::error_code
SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle);

asio::error_code
SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle);

asio::error_code
SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle);

asio::error_code
SetTCPUserTimeout(asio::ip::tcp::socket::native_handle_type handle);

asio::error_code SetSocketLinger(asio::ip::tcp::socket *socket);

asio::error_code SetSocketSndBuffer(asio::ip::tcp::socket *socket);

asio::error_code SetSocketRcvBuffer(asio::ip::tcp::socket *socket);

#endif // H_NETWORK
