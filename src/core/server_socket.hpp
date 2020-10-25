//
// server_socket.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_SERVER_SOCKET
#define H_SERVER_SOCKET

#include <stdint.h>
#include <memory>

#include "core/completion_once_callback.hpp"

class IPEndPoint;
class StreamSocket;
class ServerSocket {
 public:
  ServerSocket();
  virtual ~ServerSocket();

  // Binds the socket and starts listening. Destroys the socket to stop
  // listening.
  virtual int Listen(const IPEndPoint& address, int backlog) = 0;

#if 0
  // Binds the socket with address and port, and starts listening. It expects
  // a valid IPv4 or IPv6 address. Otherwise, it returns ERR_ADDRESS_INVALID.
  virtual int ListenWithAddressAndPort(const std::string& address_string,
                                       uint16_t port,
                                       int backlog);
#endif

  // Gets current address the socket is bound to.
  virtual int GetLocalAddress(IPEndPoint* address) const = 0;

  // Accepts connection. Callback is called when new connection is
  // accepted.
  virtual int Accept(std::unique_ptr<StreamSocket>* socket,
                     CompletionOnceCallback callback) = 0;

 private:
  ServerSocket (const ServerSocket&) = delete;
  ServerSocket& operator=(const ServerSocket&) = delete;
};

#endif // H_SERVER_SOCKET
