// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONNECTION
#define H_CONNECTION

#include <deque>
#include <functional>
#include <utility>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"
#include "network.hpp"
#include "protocol.hpp"

class Connection {
 public:
  /// Construct the connection with io context
  ///
  /// \param io_context the io context associated with the service
  Connection(asio::io_context& io_context,
             const asio::ip::tcp::endpoint& remote_endpoint)
      : io_context_(io_context),
        remote_endpoint_(remote_endpoint),
        socket_(io_context_) {}

  virtual ~Connection() = default;

  /// Construct the connection with socket
  ///
  /// \param socket the socket bound to the service
  /// \param endpoint the service socket's endpoint
  /// \param peer_endpoint the peer endpoint
  void on_accept(asio::ip::tcp::socket&& socket,
                 const asio::ip::tcp::endpoint& endpoint,
                 const asio::ip::tcp::endpoint& peer_endpoint) {
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

  asio::io_context& io_context() { return io_context_; }

 protected:
  /// the io context associated with
  asio::io_context& io_context_;
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

#endif  // H_CONNECTION
