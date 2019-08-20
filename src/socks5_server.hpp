//
// socks5_server.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SOCKS5_SERVER
#define H_SOCKS5_SERVER

#include "socks5_connection.hpp"

namespace socks5 {

class Socks5Server : public Service {
public:
  Socks5Server(boost::asio::io_context &io_context) : Service(io_context) {}
  ~Socks5Server() override {}

  void on_accept(boost::asio::ip::tcp::endpoint endpoint,
                 boost::asio::ip::tcp::socket &&socket,
                 boost::asio::ip::tcp::endpoint peer_endpoint,
                 boost::asio::ip::tcp::endpoint remote_endpoint) override {
    conn_ = std::make_shared<socks5::Socks5Connection>(
        io_context(), std::move(socket), endpoint, peer_endpoint,
        remote_endpoint);
    conn_->start();
  }

  void set_disconnect_cb(std::function<void()> cb) {
    conn_->set_disconnect_cb(cb);
  }

  void close() {
    if (conn_) {
      conn_->close();
    }
  }

private:
  std::shared_ptr<socks5::Socks5Connection> conn_;
};

typedef ServiceFactory<Socks5Server> Socks5Factory;
} // namespace socks5
#endif // H_SOCKS5_SERVER
