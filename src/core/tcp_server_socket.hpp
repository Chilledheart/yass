//
// tcp_server_socket.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_TCP_SERVER_SOCKET
#define H_TCP_SERVER_SOCKET

#include "core/server_socket.hpp"

#include "core/ip_endpoint.hpp"
#include "core/pr_util.hpp"

class TCPServerSocket : public ServerSocket {
public:
  // Adopts the provided socket, which must not be a connected socket.
  explicit TCPServerSocket(PRFileDesc *socket);

  ~TCPServerSocket() override;

  // ServerSocket implementation.
  int Listen(const IPEndPoint& address, int backlog) override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int Accept(std::unique_ptr<StreamSocket>* socket,
             CompletionOnceCallback callback) override;


 private:
  // Converts |accepted_socket_| and stores the result in
  // |output_accepted_socket|.
  // |output_accepted_socket| is untouched on failure. But |accepted_socket_| is
  // set to NULL in any case.
  int ConvertAcceptedSocket(
      int result,
      std::unique_ptr<StreamSocket>* output_accepted_socket);
  // Completion callback for calling TCPSocket::Accept().
  void OnAcceptCompleted(std::unique_ptr<StreamSocket>* output_accepted_socket,
                         CompletionOnceCallback forward_callback,
                         int result);

  PRFileDesc *socket_;

  PRFileDesc *accepted_socket_;
  IPEndPoint accepted_address_;
  bool pending_accept_;

  // DISALLOW_COPY_AND_ASSIGN
  TCPServerSocket(const TCPServerSocket&) = delete;
  TCPServerSocket& operator=(const TCPServerSocket&) = delete;
};
