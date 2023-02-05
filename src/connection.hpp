// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CONNECTION
#define H_CONNECTION

#include <deque>
#include <functional>
#include <memory>
#include <utility>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"
#include "network.hpp"
#include "protocol.hpp"

class Connection {
  using io_handle_t = std::function<void(asio::error_code, std::size_t)>;
  using handle_t = std::function<void(asio::error_code)>;
 public:
  /// Construct the connection with io context
  ///
  /// \param io_context the io context associated with the service
  /// \param remote_host_name the sni name used with remote endpoint
  /// \param remote_port the port used with remote endpoint
  /// \param upstream_https_fallback the data channel (upstream) falls back to https (alpn)
  /// \param https_fallback the data channel falls back to https (alpn)
  /// \param enable_upstream_tls the underlying data channel (upstream) is using tls
  /// \param enable_tls the underlying data channel is using tls
  /// \param upstream_ssl_ctx the ssl context object for tls data transfer (upstream)
  /// \param ssl_ctx the ssl context object for tls data transfer
  Connection(asio::io_context& io_context,
             const std::string& remote_host_name,
             uint16_t remote_port,
             bool upstream_https_fallback,
             bool https_fallback,
             bool enable_upstream_tls,
             bool enable_tls,
             asio::ssl::context *upstream_ssl_ctx,
             asio::ssl::context *ssl_ctx)
      : io_context_(&io_context),
        remote_host_name_(remote_host_name),
        remote_port_(remote_port),
        socket_(*io_context_),
        upstream_https_fallback_(upstream_https_fallback),
        https_fallback_(https_fallback),
        enable_upstream_tls_(enable_upstream_tls),
        enable_tls_(enable_tls),
        upstream_ssl_ctx_(upstream_ssl_ctx),
        ssl_socket_(socket_, *ssl_ctx) {
    if (enable_tls) {
      setup_ssl();
      s_async_read_some_ = [this](io_handle_t cb) {
        ssl_socket_.async_read_some(asio::null_buffers(), cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_.read_some(mutable_buffer(*buf), ec);
      };
      s_async_write_some_ = [this](io_handle_t cb) {
        ssl_socket_.async_write_some(asio::null_buffers(), cb);
      };
      s_write_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_.write_some(const_buffer(*buf), ec);
      };
      s_async_shutdown_ = [this](handle_t cb) {
        ssl_socket_.async_shutdown(cb);
      };
      s_shutdown_ = [this](asio::error_code &ec) {
        // FIXME use async_shutdown correctly
        ssl_socket_.shutdown(ec);
      };
    } else {
      s_async_read_some_ = [this](io_handle_t cb) {
        socket_.async_read_some(asio::null_buffers(), cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return socket_.read_some(mutable_buffer(*buf), ec);
      };
      s_async_write_some_ = [this](io_handle_t cb) {
        socket_.async_write_some(asio::null_buffers(), cb);
      };
      s_write_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return socket_.write_some(const_buffer(*buf), ec);
      };
      s_async_shutdown_ = [this](handle_t cb) {
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        cb(ec);
      };
      s_shutdown_ = [this](asio::error_code &ec) {
        // FIXME use async_shutdown correctly
        socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
      };
    }
  }

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  Connection(Connection&&) = default;
  Connection& operator=(Connection&&) = default;

  virtual ~Connection() = default;

  void set_https_fallback(bool https_fallback) { https_fallback_ = https_fallback; }

 private:
  void setup_ssl() {
    SSL* ssl = ssl_socket_.native_handle();
    SSL_set_shed_handshake_config(ssl, 1);
    ssl_socket_.set_verify_mode(asio::ssl::verify_peer);
  }

 public:
  /// Construct the connection with socket
  ///
  /// \param socket the socket bound to the service
  /// \param endpoint the service socket's endpoint
  /// \param peer_endpoint the peer endpoint
  /// \param the number of connection id
  void on_accept(asio::ip::tcp::socket&& socket,
                 const asio::ip::tcp::endpoint& endpoint,
                 const asio::ip::tcp::endpoint& peer_endpoint,
                 int connection_id) {
    socket_ = std::move(socket);
    endpoint_ = endpoint;
    peer_endpoint_ = peer_endpoint;
    connection_id_ = connection_id;
  }

  /// Enter the start phase, begin to read requests
  virtual void start() {}

  /// Close the socket and clean up
  virtual void close() {}

  /// set callback
  ///
  /// \param cb the callback function pointer when disconnect happens
  void set_disconnect_cb(std::function<void()> cb) { disconnect_cb_ = cb; }

  asio::io_context& io_context() { return *io_context_; }

  const asio::ip::tcp::endpoint& endpoint() const { return endpoint_; }

  const asio::ip::tcp::endpoint& peer_endpoint() const {
    return peer_endpoint_;
  }

  int connection_id() const {
    return connection_id_;
  }

 protected:
  /// the io context associated with
  asio::io_context* io_context_;
  /// the upstream host name to be established with
  std::string remote_host_name_;
  /// the upstream port to be established with
  uint16_t remote_port_;

  /// the socket the service bound with
  asio::ip::tcp::socket socket_;
  /// service's bound endpoint
  asio::ip::tcp::endpoint endpoint_;
  /// the peer endpoint the connection connects
  asio::ip::tcp::endpoint peer_endpoint_;
  /// the number of connection id
  int connection_id_ = -1;

  /// if https fallback
  bool upstream_https_fallback_;
  bool https_fallback_;
  /// if enable ssl layer
  bool enable_upstream_tls_;
  bool enable_tls_;
  std::string upstream_certificate_;
  asio::ssl::context* upstream_ssl_ctx_;
  asio::ssl::stream<asio::ip::tcp::socket&> ssl_socket_;

  /// io_handlers
  std::function<void(io_handle_t)> s_async_read_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_read_some_;
  std::function<void(io_handle_t)> s_async_write_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_write_some_;
  std::function<void(handle_t)> s_async_shutdown_;
  std::function<void(asio::error_code&)> s_shutdown_;

  /// the callback invoked when disconnect event happens
  std::function<void()> disconnect_cb_;

 protected:
  /// statistics of read bytes
  size_t rbytes_transferred_ = 0;
  /// statistics of written bytes
  size_t wbytes_transferred_ = 0;
};

class ConnectionFactory {
 public:
   virtual const char* Name() = 0;
   virtual const char* ShortName() = 0;
};

#endif  // H_CONNECTION
