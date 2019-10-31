//
// service.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SERVICE
#define H_SERVICE

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
#include "protocol.hpp"

class Service {
public:
  Service(boost::asio::io_context &io_context) : io_context_(io_context) {}

  boost::asio::io_context &io_context() { return io_context_; }

  virtual ~Service() {}

  virtual void on_accept(boost::asio::ip::tcp::endpoint endpoint,
                         boost::asio::ip::tcp::socket &&socket,
                         boost::asio::ip::tcp::endpoint peer_endpoint,
                         boost::asio::ip::tcp::endpoint remote_endpoint) = 0;

private:
  boost::asio::io_context &io_context_;
};

template <class T> class ServiceFactory {
public:
  ServiceFactory(boost::asio::io_context &io_context)
      : io_context_(io_context) {}

  void listen(const boost::asio::ip::tcp::endpoint &endpoint,
              const boost::asio::ip::tcp::endpoint &remote_endpoint) {
    endpoint_ = endpoint;
    remote_endpoint_ = remote_endpoint;
    acceptor_ =
        std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_, endpoint);
    if (FLAGS_reuse_port) {
      acceptor_->set_option(
          boost::asio::ip::tcp::acceptor::reuse_address(true));
    }
    LOG(WARNING) << "listen to " << endpoint_ << " with upstream "
                 << remote_endpoint_;
    startAccept();
  }

  void stop() {
    std::vector<std::shared_ptr<T>> conns = connections_;
    acceptor_->cancel();
    for (auto conn : conns) {
      conn->close();
    }
  }

  size_t currentConnections() const { return connections_.size(); }

private:
  void startAccept() {
    std::shared_ptr<T> conn = std::make_unique<T>(io_context_);
    acceptor_->async_accept(peer_endpoint_,
                            std::bind(&ServiceFactory<T>::handleAccept, this,
                                      conn, std::placeholders::_1,
                                      std::placeholders::_2));
  }

  void handleAccept(std::shared_ptr<T> conn, boost::system::error_code error,
                    boost::asio::ip::tcp::socket socket) {
    if (!error) {
      conn->on_accept(peer_endpoint_, std::move(socket), peer_endpoint_,
                      remote_endpoint_);
      conn->set_disconnect_cb(
          [this, conn]() mutable { handleDisconnect(conn); });
      connections_.push_back(conn);
      LOG(WARNING) << "accepted a new connection total: "
                   << connections_.size();
      startAccept();
    }
  }

  void handleDisconnect(std::shared_ptr<T> conn) {
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end());
    LOG(WARNING) << "disconnected connection with remaining: "
                 << connections_.size();
    conn->close();
  }

private:
  boost::asio::io_context &io_context_;
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::endpoint peer_endpoint_;

  boost::asio::ip::tcp::endpoint remote_endpoint_;

  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  std::vector<std::shared_ptr<T>> connections_;
};

#endif // H_SERVICE
