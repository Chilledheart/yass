// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

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
  /// \param remote_endpoint the remote endpoint of the service socket
  Connection(asio::io_context& io_context,
             const asio::ip::tcp::endpoint& remote_endpoint,
             bool enable_ssl)
      : io_context_(&io_context),
        remote_endpoint_(remote_endpoint),
        socket_(*io_context_),
        enable_ssl_(enable_ssl),
        ssl_ctx_(asio::ssl::context::tls_server),
        ssl_socket_(socket_, ssl_ctx_) {
    if (enable_ssl) {
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

 public:
  void load_upstream_certificate(const std::string &upstream_certificate) {
    upstream_certificate_ = upstream_certificate;
  }

  asio::error_code load_certificate(const std::string &cert) {
    asio::error_code ec;
    if (cert.empty()) {
      return ec;
    }
    ssl_ctx_.use_certificate(asio::const_buffer(cert.data(), cert.size()),
                             asio::ssl::context::pem,
                             ec);
    if (!ec) {
      VLOG(2) << "Loaded certificate";
    }
    return ec;
  }

  asio::error_code load_private_key(const std::string &pkey) {
    asio::error_code ec;
    if (pkey.empty()) {
      return ec;
    }
    ssl_ctx_.use_private_key(asio::const_buffer(pkey.data(), pkey.size()),
                             asio::ssl::context::pem,
                             ec);
    if (!ec) {
      VLOG(2) << "Loaded privated key";
    }
    return ec;
  }

 private:
  void setup_ssl() {
    load_ca_to_ssl_ctx(ssl_ctx_);
    asio::error_code ec;
    ssl_ctx_.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_tlsv1_1, ec);
    std::string certificate_chain_file = absl::GetFlag(FLAGS_certificate_chain_file);
    std::string private_key_file = absl::GetFlag(FLAGS_private_key_file);
    if (!private_key_file.empty()) {
      ssl_ctx_.set_password_callback([](size_t max_length,
                                        asio::ssl::context::password_purpose purpose) {
        return absl::GetFlag(FLAGS_private_key_password);
      });
      ssl_ctx_.use_certificate_chain_file(certificate_chain_file, ec);
      if (!ec) {
        VLOG(2) << "Using certificate file: " << certificate_chain_file;
      }
      ssl_ctx_.use_private_key_file(private_key_file, asio::ssl::context::pem, ec);
      if (!ec) {
        VLOG(2) << "Using private key file: " << private_key_file;
      }
    }
    ssl_socket_.set_verify_mode(asio::ssl::verify_peer);

    SSL_CTX *ctx = ssl_ctx_.native_handle();
    SSL_CTX_set_alpn_select_cb(ctx, &Connection::on_alpn_select, nullptr);
  }

  static int on_alpn_select(SSL *ssl,
                            const unsigned char **out,
                            unsigned char *outlen,
                            const unsigned char *in,
                            unsigned int inlen,
                            void *arg) {
    unsigned char pos = 0;
    while (pos < inlen) {
      if (in[0] + 1 > inlen) {
        goto err;
      }
      if (std::string(reinterpret_cast<const char*>(in + 1), in[0]) == "h2") {
        *out = in + 1;
        *outlen = in[0];
        return SSL_TLSEXT_ERR_OK;
      }
      pos += in[0];
      in += in[0];
    }

  err:
    return SSL_TLSEXT_ERR_ALERT_FATAL;
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
  /// the upstream endpoint to be established with
  asio::ip::tcp::endpoint remote_endpoint_;

  /// the socket the service bound with
  asio::ip::tcp::socket socket_;
  /// service's bound endpoint
  asio::ip::tcp::endpoint endpoint_;
  /// the peer endpoint the connection connects
  asio::ip::tcp::endpoint peer_endpoint_;
  /// the number of connection id
  int connection_id_ = -1;

  /// if enable ssl
  bool enable_ssl_;
  asio::ssl::context ssl_ctx_;
  asio::ssl::stream<asio::ip::tcp::socket&> ssl_socket_;
  std::string upstream_certificate_;

  /// io_handlers
  std::function<void(io_handle_t)> s_async_read_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_read_some_;
  std::function<void(io_handle_t)> s_async_write_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_write_some_;
  std::function<void(handle_t)> s_async_shutdown_;
  std::function<void(asio::error_code&)> s_shutdown_;

  /// the callback invoked when disconnect event happens
  std::function<void()> disconnect_cb_;
};

class ConnectionFactory {
 public:
   virtual const char* Name() = 0;
   virtual const char* ShortName() = 0;
};

#endif  // H_CONNECTION
