//
// connection.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONNECTION
#define H_CONNECTION

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <deque>
#include <functional>
#include <utility>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "protocol.hpp"

class Connection {
public:
  /// Construct the connection with io context
  ///
  /// \param io_context the io context associated with the service
  Connection(asio::io_context &io_context,
             const asio::ip::tcp::endpoint &remote_endpoint)
      : io_context_(io_context), remote_endpoint_(remote_endpoint),
        socket_(io_context_) {}

  virtual ~Connection() {}

  /// Construct the connection with socket
  ///
  /// \param socket the socket bound to the service
  /// \param endpoint the service socket's endpoint
  /// \param peer_endpoint the peer endpoint
  void on_accept(asio::ip::tcp::socket &&socket,
                 const asio::ip::tcp::endpoint &endpoint,
                 const asio::ip::tcp::endpoint &peer_endpoint) {
    socket_ = std::move(socket);
    endpoint_ = endpoint;
    peer_endpoint_ = peer_endpoint;
  }

  /// Enter the start phase, begin to read requests
  virtual void start() {}

  /// Close the socket and clean up
  virtual void close() {}

  /// set callback
  void set_disconnect_cb(std::function<void()> cb) { disconnect_cb_ = cb; }

  asio::io_context &io_context() { return io_context_; }

protected:
  /// the io context associated with
  asio::io_context &io_context_;
  /// the upstream endpoint to be established with
  asio::ip::tcp::endpoint remote_endpoint_;

  /// the socket the service bound with
  asio::ip::tcp::socket socket_;
  /// service's bound endpoint
  asio::ip::tcp::endpoint endpoint_;
  /// the peer endpoint the connection connects
  asio::ip::tcp::endpoint peer_endpoint_;

  /// the callback invoked when disconnect event happens
  std::function<void()> disconnect_cb_;
};

#endif // H_CONNECTION
