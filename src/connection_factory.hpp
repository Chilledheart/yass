// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CONNECTION_FACTORY
#define H_CONNECTION_FACTORY

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <algorithm>
#include <vector>
#include <functional>
#include <utility>

#include "config/config.hpp"
#include "connection.hpp"
#include "core/asio.hpp"
#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "network.hpp"

template <class T>
class ServiceFactory {
  class SlaveContext {
   public:
    SlaveContext(size_t slave_index,
                 const asio::ip::tcp::endpoint& remote_endpoint)
        : index_(slave_index),
          work_guard_(std::make_unique<asio::io_context::work>(io_context_)),
          thread_([this]() { WorkFunc(); }),
          remote_endpoint_(remote_endpoint) {}

    // Allow called from different threads
    ~SlaveContext() noexcept {
      work_guard_.reset();
      thread_.join();
    }

    SlaveContext(const SlaveContext&) noexcept = delete;
    SlaveContext(SlaveContext&&) noexcept = default;

    void listen(const asio::ip::tcp::endpoint& endpoint,
                int backlog,
                asio::error_code& ec) {
      if (acceptor_) {
        ec = asio::error::already_started;
        return;
      }
      endpoint_ = endpoint;
      acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_context_);

      acceptor_->open(endpoint.protocol(), ec);
      if (ec) {
        return;
      }
      if (absl::GetFlag(FLAGS_reuse_port)) {
        acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
        SetSOReusePort(acceptor_->native_handle(), ec);
      }
      if (ec) {
        return;
      }
      SetTCPFastOpen(acceptor_->native_handle(), ec);
      if (ec) {
        return;
      }
      acceptor_->bind(endpoint, ec);
      if (ec) {
        return;
      }
      acceptor_->listen(backlog, ec);
      if (ec) {
        return;
      }
      VLOG(2) << "slave " << index_ << " listen to " << endpoint_
              << " with upstream " << remote_endpoint_;
      io_context_.post([this]() { startAccept(); });
    }

    // Allow called from different threads
    void stop() {
      io_context_.post([this]() {
        if (acceptor_) {
          asio::error_code ec;
          acceptor_->close(ec);
          if (ec) {
            VLOG(2) << "Acceptor close failed: " << ec;
          }
        }

        std::vector<std::shared_ptr<T>> conns = std::move(connections_);
        for (auto conn : conns) {
          conn->close();
        }

        acceptor_.reset();
      });
    }

    // Allow called from different threads
    size_t currentConnections() const {
      return connections_.size();
    }

   private:
    void WorkFunc() {
      asio::error_code ec;
      std::string slave_name = absl::StrCat("slave-", std::to_string(index_));
      if (!SetThreadName(thread_.native_handle(), slave_name)) {
        PLOG(WARNING) << "slave " << index_ << " failed to set thread name";
      }
      if (!SetThreadPriority(thread_.native_handle(),
                             ThreadPriority::ABOVE_NORMAL)) {
        PLOG(WARNING) << "slave " << index_ << " failed to set thread priority";
      }
      io_context_.run(ec);

      if (ec) {
        LOG(ERROR) << "slave " << index_ << " io_context failed due to: " << ec;
      }
    }

    void startAccept() {
      acceptor_->async_accept(
          peer_endpoint_,
          [this](asio::error_code error, asio::ip::tcp::socket socket) {
            std::shared_ptr<T> conn =
                std::make_unique<T>(io_context_, remote_endpoint_);
            handleAccept(conn, error, std::move(socket));
          });
    }

    void handleAccept(std::shared_ptr<T> conn,
                      asio::error_code ec,
                      asio::ip::tcp::socket socket) {
      if (!ec) {
        SetTCPCongestion(socket.native_handle(), ec);
        SetTCPUserTimeout(socket.native_handle(), ec);
        SetSocketLinger(&socket, ec);
        SetSocketSndBuffer(&socket, ec);
        SetSocketRcvBuffer(&socket, ec);
        conn->on_accept(std::move(socket), endpoint_, peer_endpoint_);
        conn->set_disconnect_cb(
            [this, conn]() mutable { handleDisconnect(conn); });
        connections_.push_back(conn);
        conn->start();
        VLOG(2) << "slave " << index_
                << " connection established with remaining: "
                << connections_.size();
        startAccept();
      }
      if (ec != asio::error::operation_aborted) {
        VLOG(2) << "slave " << index_ << " stopping accept due to " << ec;
      }
    }

    void handleDisconnect(std::shared_ptr<T> conn) {
      VLOG(2) << "slave " << index_
              << " connection closed with remaining: " << connections_.size();
      connections_.erase(
          std::remove(connections_.begin(), connections_.end(), conn),
          connections_.end());
    }

   private:
    size_t index_;
    asio::io_context io_context_;
    /// stopping the io_context from running out of work
    std::unique_ptr<asio::io_context::work> work_guard_;
    std::thread thread_;

    const asio::ip::tcp::endpoint remote_endpoint_;

    asio::ip::tcp::endpoint endpoint_;
    asio::ip::tcp::endpoint peer_endpoint_;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;

    std::vector<std::shared_ptr<T>> connections_;
  };

 public:
  ServiceFactory(const asio::ip::tcp::endpoint& remote_endpoint) {
    size_t num_of_threads = absl::GetFlag(FLAGS_threads);
    slaves_.reserve(num_of_threads);
    for (size_t i = 0; i < num_of_threads; ++i) {
      slaves_.emplace_back(std::make_unique<SlaveContext>(i, remote_endpoint));
    }
  }

  ~ServiceFactory() { join(); }

  void listen(const asio::ip::tcp::endpoint& endpoint,
              int backlog,
              asio::error_code& ec) {
    for (auto& slave : slaves_) {
      slave->listen(endpoint, backlog, ec);
      if (ec)
        break;
    }
  }

  void stop() {
    for (auto& slave : slaves_) {
      slave->stop();
    }
  }

  void join() {
    slaves_.clear();
  }

  size_t currentConnections() const {
    size_t count = 0;
    for (const auto& slave : slaves_) {
      count += slave->currentConnections();
    }
    return count;
  }

 private:
  std::vector<std::unique_ptr<SlaveContext>> slaves_;
};

#endif  // H_CONNECTION_FACTORY
