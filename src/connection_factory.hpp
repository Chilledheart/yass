//
// connection_factory.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CONNECTION_FACTORY
#define H_CONNECTION_FACTORY

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <deque>
#include <functional>
#include <glog/logging.h>
#include <utility>

#include "config.hpp"
#include "connection.hpp"

template <class T> class ServiceFactory {
public:
  ServiceFactory(boost::asio::io_context &io_context,
                 const boost::asio::ip::tcp::endpoint &remote_endpoint)
      : io_context_(io_context), remote_endpoint_(remote_endpoint) {}

  boost::system::error_code listen(const boost::asio::ip::tcp::endpoint &endpoint) {
    endpoint_ = endpoint;
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_);

    boost::system::error_code ec = boost::system::error_code();
    acceptor_->open(endpoint.protocol(), ec);
    if (ec) {
      return ec;
    }
    if (FLAGS_reuse_port) {
      acceptor_->set_option(
          boost::asio::ip::tcp::acceptor::reuse_address(true));
    }
    acceptor_->bind(endpoint, ec);
    if (ec) {
      return ec;
    }
    acceptor_->listen(1 /*backlog*/, ec);
    if (ec) {
      return ec;
    }
    LOG(WARNING) << "listen to " << endpoint_ << " with upstream "
                 << remote_endpoint_;
    startAccept();
    return ec;
  }

  void stop() {
    std::vector<std::shared_ptr<T>> conns = std::move(connections_);
    if (acceptor_) {
      acceptor_->cancel();
      acceptor_.reset();
    }
    for (auto conn : conns) {
      conn->close();
    }
  }

  size_t currentConnections() const { return connections_.size(); }

private:
  void startAccept() {
    std::shared_ptr<T> conn =
        std::make_unique<T>(io_context_, remote_endpoint_);
    acceptor_->async_accept(peer_endpoint_,
                            std::bind(&ServiceFactory<T>::handleAccept, this,
                                      conn, std::placeholders::_1,
                                      std::placeholders::_2));
  }

  void handleAccept(std::shared_ptr<T> conn, boost::system::error_code error,
                    boost::asio::ip::tcp::socket socket) {
    if (!error) {
      conn->on_accept(std::move(socket), endpoint_, peer_endpoint_);
      conn->set_disconnect_cb(
          [this, conn]() mutable { handleDisconnect(conn); });
      connections_.push_back(conn);
      conn->start();
      VLOG(2) << "connection established with remaining: "
              << connections_.size();
      startAccept();
    }
  }

  void handleDisconnect(std::shared_ptr<T> conn) {
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end());
    VLOG(2) << "connection closed with remaining: " << connections_.size();
    conn->set_disconnect_cb(std::function<void()>());
    conn->close();
  }

private:
  boost::asio::io_context &io_context_;
  const boost::asio::ip::tcp::endpoint remote_endpoint_;

  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::endpoint peer_endpoint_;

  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  std::vector<std::shared_ptr<T>> connections_;
};

#endif // H_CONNECTION_FACTORY
