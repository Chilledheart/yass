// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#ifndef H_CONTENT_SERVER
#define H_CONTENT_SERVER

#include <absl/flags/flag.h>
#include <absl/strings/str_format.h>
#include <algorithm>
#include <atomic>
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

/// An interface used to provide service
template <typename T>
class ContentServer {
 public:
  using ConnectionType = typename T::ConnectionType;
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnConnect(int connection_id) = 0;
    virtual void OnDisconnect(int connection_id) = 0;
  };

 private:
  class ThreadedContext {
   public:
    ThreadedContext(size_t threaded_context_index,
                    const asio::ip::tcp::endpoint& remote_endpoint,
                    std::atomic<int> *next_connection_id,
                    ContentServer::Delegate *delegate)
        : index_(threaded_context_index),
          work_guard_(std::make_unique<asio::io_context::work>(io_context_)),
          thread_([this]() { do_accept_loop(); }),
          remote_endpoint_(remote_endpoint),
          next_connection_id_(next_connection_id),
          delegate_(delegate) {}

    /// Disconstructor allow called from different threads
    ~ThreadedContext() noexcept {
      work_guard_.reset();
      thread_.join();
    }

    ThreadedContext(const ThreadedContext&) = delete;
    ThreadedContext& operator=(const ThreadedContext&) = delete;

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
      LOG(INFO) << "Thread " << index_ << " Listening (" << factory_.Name() << ") on "
                << endpoint_;
      io_context_.post([this]() { accept(); });
    }

    // Allow called from different threads
    void stop() {
      io_context_.post([this]() {
        if (acceptor_) {
          asio::error_code ec;
          acceptor_->close(ec);
          if (ec) {
            LOG(ERROR) << "Thread " << index_ << " (" << factory_.Name() << ")"
                       << " acceptor close failed: " << ec;
          }
        }

        std::vector<std::shared_ptr<ConnectionType>> conns = std::move(connections_);
        for (auto conn : conns) {
          conn->close();
        }

        acceptor_.reset();
      });
    }

   private:
    void do_accept_loop() {
      asio::error_code ec;
      std::string threaded_context_name = absl::StrFormat("%s-thread-%d",
        factory_.ShortName(), index_);
      if (!SetThreadName(thread_.native_handle(), threaded_context_name)) {
        PLOG(WARNING) << "Thread " << index_ << " (" << factory_.Name() << ")"
                      << " failed to set thread name";
      }
      if (!SetThreadPriority(thread_.native_handle(),
                             ThreadPriority::ABOVE_NORMAL)) {
        PLOG(WARNING) << "Thread " << index_ << " (" << factory_.Name() << ")"
                      << " failed to set thread priority";
      }
      io_context_.run(ec);

      if (ec) {
        LOG(ERROR) << "Thread " << index_ << " (" << factory_.Name() << ")"
                   << " failed to accept more due to: " << ec;
      }
    }

    void accept() {
      acceptor_->async_accept(
          peer_endpoint_,
          [this](asio::error_code error, asio::ip::tcp::socket socket) {
            std::shared_ptr<ConnectionType> conn =
                factory_.Create(io_context_, remote_endpoint_);
            on_accept(conn, error, std::move(socket));
          });
    }

    void on_accept(std::shared_ptr<ConnectionType> conn,
                   asio::error_code ec,
                   asio::ip::tcp::socket socket) {
      if (!ec) {
        int connection_id = *next_connection_id_++;
        SetTCPCongestion(socket.native_handle(), ec);
        SetTCPConnectionTimeout(socket.native_handle(), ec);
        SetTCPUserTimeout(socket.native_handle(), ec);
        SetSocketLinger(&socket, ec);
        SetSocketSndBuffer(&socket, ec);
        SetSocketRcvBuffer(&socket, ec);
        conn->on_accept(std::move(socket), endpoint_, peer_endpoint_,
                        connection_id);
        conn->set_disconnect_cb(
            [this, conn]() mutable { on_disconnect(conn); });
        connections_.push_back(conn);
        if (delegate_) {
          delegate_->OnConnect(connection_id);
        }
        LOG(INFO) << "Connection (" << factory_.Name() << ") "
                  << connection_id << " with " << conn->peer_endpoint()
                  << " connected";
        conn->start();
        accept();
      } if (ec != asio::error::operation_aborted) {
        LOG(ERROR) << "Thread " << index_ << " (" << factory_.Name() << ")"
                   << " failed to accept more due to: " << ec;
      }
    }

    void on_disconnect(std::shared_ptr<typename T::ConnectionType> conn) {
      int connection_id = conn->connection_id();
      LOG(INFO) << "Connection (" << factory_.Name() << ") "
                << connection_id << " disconnected";
      connections_.erase(
          std::remove(connections_.begin(), connections_.end(), conn),
          connections_.end());
      if (delegate_) {
        delegate_->OnDisconnect(connection_id);
      }
    }

   private:
    size_t index_;
    asio::io_context io_context_;
    /// stopping the io_context from running out of work
    std::unique_ptr<asio::io_context::work> work_guard_;
    std::thread thread_;

    const asio::ip::tcp::endpoint remote_endpoint_;
    std::atomic<int> *next_connection_id_;
    ContentServer::Delegate *delegate_;

    asio::ip::tcp::endpoint endpoint_;
    asio::ip::tcp::endpoint peer_endpoint_;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;

    std::vector<std::shared_ptr<typename T::ConnectionType>> connections_;

    T factory_;
  };

 public:
  ContentServer(const asio::ip::tcp::endpoint& remote_endpoint,
                ContentServer::Delegate *delegate = nullptr) {
    size_t num_of_threads = absl::GetFlag(FLAGS_threads);
    threaded_contexts_.reserve(num_of_threads);
    for (size_t i = 0; i < num_of_threads; ++i) {
      threaded_contexts_.emplace_back(
          std::make_unique<ThreadedContext>(i, remote_endpoint,
                                            &next_connection_id_, delegate));
    }
  }

  ~ContentServer() { join(); }

  ContentServer(const ContentServer&) = delete;
  ContentServer& operator=(const ContentServer&) = delete;

  void listen(const asio::ip::tcp::endpoint& endpoint,
              int backlog,
              asio::error_code& ec) {
    for (auto& threaded_context : threaded_contexts_) {
      threaded_context->listen(endpoint, backlog, ec);
      if (ec)
        break;
    }
  }

  void stop() {
    for (auto& threaded_context : threaded_contexts_) {
      threaded_context->stop();
    }
  }

  void join() {
    threaded_contexts_.clear();
  }

  size_t num_of_connections() const {
    return next_connection_id_ - 1;
  }

 private:
  std::vector<std::unique_ptr<ThreadedContext>> threaded_contexts_;
  std::atomic<int> next_connection_id_ = 1;
};

#endif  // H_CONTENT_SERVER
