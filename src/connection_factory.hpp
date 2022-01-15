// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONNECTION_FACTORY
#define H_CONNECTION_FACTORY

#include <absl/flags/flag.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <utility>

#include "config/config.hpp"
#include "connection.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"
#include "network.hpp"

template <class T>
class ServiceFactory {
 public:
  ServiceFactory(asio::io_context& io_context,
                 const asio::ip::tcp::endpoint& remote_endpoint)
      : io_context_(io_context), remote_endpoint_(remote_endpoint) {}

  asio::error_code listen(const asio::ip::tcp::endpoint& endpoint,
                          int backlog,
                          asio::error_code& ec) {
    endpoint_ = endpoint;
    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_);

    acceptor_->open(endpoint.protocol(), ec);
    if (ec) {
      return ec;
    }
    if (absl::GetFlag(FLAGS_reuse_port)) {
      acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    }
    SetTCPFastOpen(acceptor_->native_handle());
    if (ec) {
      return ec;
    }
    acceptor_->bind(endpoint, ec);
    if (ec) {
      return ec;
    }
    acceptor_->listen(backlog, ec);
    if (ec) {
      return ec;
    }
    LOG(WARNING) << "listen to " << endpoint_ << " with upstream "
                 << remote_endpoint_;
    startAccept();
    return ec;
  }

  void stop() {
    if (acceptor_) {
      asio::error_code ec = asio::error_code();
      acceptor_->close(ec);
    }
    std::vector<std::shared_ptr<T>> conns = std::move(connections_);
    for (auto conn : conns) {
      conn->close();
    }
    acceptor_.reset();
  }

  size_t currentConnections() const { return connections_.size(); }

 private:
  void startAccept() {
    std::shared_ptr<T> conn =
        std::make_unique<T>(io_context_, remote_endpoint_);
    acceptor_->async_accept(
        peer_endpoint_,
        [this, conn](asio::error_code error, asio::ip::tcp::socket socket) {
          handleAccept(conn, error, std::move(socket));
        });
  }

  void handleAccept(std::shared_ptr<T> conn,
                    asio::error_code error,
                    asio::ip::tcp::socket socket) {
    if (!error) {
      SetTCPCongestion(socket.native_handle());
      SetTCPUserTimeout(socket.native_handle());
      SetSocketLinger(&socket);
      SetSocketSndBuffer(&socket);
      SetSocketRcvBuffer(&socket);
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
    VLOG(2) << "connection closed with remaining: " << connections_.size();
    conn->set_disconnect_cb(std::function<void()>());
    conn->close();
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end());
  }

 private:
  asio::io_context& io_context_;
  const asio::ip::tcp::endpoint remote_endpoint_;

  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::endpoint peer_endpoint_;

  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::vector<std::shared_ptr<T>> connections_;
};

#endif  // H_CONNECTION_FACTORY
