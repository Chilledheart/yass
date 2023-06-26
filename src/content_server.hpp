// SPDX-License-Identifier: GPL-2.0 Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_CONTENT_SERVER
#define H_CONTENT_SERVER

#include <absl/container/flat_hash_map.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_format.h>
#include <algorithm>
#include <array>
#include <vector>
#include <functional>
#include <utility>

#include "config/config.hpp"
#include "connection.hpp"
#include "core/asio.hpp"
#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "network.hpp"

#define MAX_LISTEN_ADDRESSES 30

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

 public:
  explicit ContentServer(asio::io_context &io_context,
                         const std::string& remote_host_name = {},
                         uint16_t remote_port = {},
                         const std::string& upstream_certificate = {},
                         const std::string& certificate = {},
                         const std::string& private_key = {},
                         ContentServer::Delegate *delegate = nullptr)
    : io_context_(io_context),
      work_guard_(std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor())),
      remote_host_name_(remote_host_name),
      remote_port_(remote_port),
      upstream_https_fallback_(absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTPS),
      https_fallback_(absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTPS),
      enable_upstream_tls_(
          absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTPS ||
          absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2_TLS),
      enable_tls_(
          absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTPS ||
          absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2_TLS),
      upstream_certificate_(upstream_certificate),
      upstream_ssl_ctx_(asio::ssl::context::tls_client),
      certificate_(certificate),
      private_key_(private_key),
      ssl_ctx_(asio::ssl::context::tls_server),
      delegate_(delegate) {
    upstream_https_fallback_ &= std::string(factory_.Name()) == "client";
    https_fallback_ &= std::string(factory_.Name()) == "server";
    enable_upstream_tls_ &= std::string(factory_.Name()) == "client";
    enable_tls_ &= std::string(factory_.Name()) == "server";
  }

  ~ContentServer() {
    work_guard_.reset();
  }

  ContentServer(const ContentServer&) = delete;
  ContentServer& operator=(const ContentServer&) = delete;

  // Retrieve last local endpoint
  const asio::ip::tcp::endpoint& endpoint() const {
    DCHECK_NE(next_listen_ctx_, 0) << "Server should listen to some address";
    return listen_ctxs_[next_listen_ctx_ - 1].endpoint;
  }

  void listen(const asio::ip::tcp::endpoint& endpoint,
              int backlog,
              asio::error_code& ec) {
    if (next_listen_ctx_ >= MAX_LISTEN_ADDRESSES) {
      ec = asio::error::already_started;
      return;
    }
    ListenCtx& ctx = listen_ctxs_[next_listen_ctx_];
    ctx.endpoint = endpoint;
    ctx.acceptor = std::make_unique<asio::ip::tcp::acceptor>(io_context_);

    ctx.acceptor->open(endpoint.protocol(), ec);
    if (ec) {
      return;
    }
    if (absl::GetFlag(FLAGS_reuse_port)) {
      ctx.acceptor->set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
      SetSOReusePort(ctx.acceptor->native_handle(), ec);
    }
    if (ec) {
      return;
    }
    SetTCPFastOpen(ctx.acceptor->native_handle(), ec);
    if (ec) {
      return;
    }
    ctx.acceptor->bind(endpoint, ec);
    if (ec) {
      return;
    }
    ctx.acceptor->listen(backlog, ec);
    if (ec) {
      return;
    }
    ctx.endpoint = ctx.acceptor->local_endpoint(ec);
    if (ec) {
      return;
    }
    if (enable_upstream_tls_) {
      setup_upstream_ssl_ctx(ec);
      if (ec) {
        return;
      }
    }
    if (enable_tls_) {
      setup_ssl_ctx(ec);
      if (ec) {
        return;
      }
    }
    LOG(INFO) << "Listening (" << factory_.Name() << ") on "
              << ctx.endpoint;
    int listen_ctx_num = next_listen_ctx_++;
    asio::post(io_context_ ,[this, listen_ctx_num]() { accept(listen_ctx_num); });
  }

  // Allow called from different threads
  void stop() {
    asio::post(io_context_, [this]() {
      for (int i = 0; i < next_listen_ctx_; ++i) {
        ListenCtx& ctx = listen_ctxs_[i];
        if (ctx.acceptor) {
          asio::error_code ec;
          ctx.acceptor->close(ec);
          ctx.acceptor.reset();
          if (ec) {
            LOG(WARNING) << "Connections (" << factory_.Name() << ")"
                         << " acceptor (" << ctx.endpoint << ") close failed: " << ec;
          }
        }
      }

      auto connection_map = std::move(connection_map_);
      for (auto [conn_id, conn] : connection_map) {
        VLOG(1) << "Connections (" << factory_.Name() << ")"
                << " closing Connection: " << conn_id;
        conn->close();
      }

      work_guard_.reset();
    });
  }

  size_t num_of_connections() const {
    return next_connection_id_ - 1;
  }

 private:
  void accept(int listen_ctx_num) {
    ListenCtx& ctx = listen_ctxs_[listen_ctx_num];
    ctx.acceptor->async_accept(
        ctx.peer_endpoint,
        [this, listen_ctx_num](asio::error_code ec, asio::ip::tcp::socket socket) {
          // acceptor->close might return success as well
          ListenCtx& ctx = listen_ctxs_[listen_ctx_num];
          if (!ctx.acceptor) {
            return;
          }
          if (!ec) {
            if (enable_tls_) {
              setup_ssl_ctx_alpn_cb();
            }
            scoped_refptr<ConnectionType> conn = factory_.Create(
              io_context_, remote_host_name_, remote_port_,
              upstream_https_fallback_, https_fallback_,
              enable_upstream_tls_, enable_tls_,
              &upstream_ssl_ctx_, &ssl_ctx_);
            on_accept(conn, std::move(socket), listen_ctx_num);
            accept(listen_ctx_num);
          } if (ec && ec != asio::error::operation_aborted) {
            LOG(WARNING) << "Acceptor (" << factory_.Name() << ")"
                         << " failed to accept more due to: " << ec;
            work_guard_.reset();
          }
        });
  }

  void on_accept(scoped_refptr<ConnectionType> conn,
                 asio::ip::tcp::socket &&socket,
                 int listen_ctx_num) {
    asio::error_code ec;
    ListenCtx& ctx = listen_ctxs_[listen_ctx_num];

    int connection_id = next_connection_id_++;
    socket.native_non_blocking(true, ec);
    socket.non_blocking(true, ec);
    SetTCPCongestion(socket.native_handle(), ec);
    SetTCPKeepAlive(socket.native_handle(), ec);
    SetSocketTcpNoDelay(&socket, ec);
    conn->on_accept(std::move(socket), ctx.endpoint, ctx.peer_endpoint,
                    connection_id);
    conn->set_disconnect_cb(
        [this, conn]() mutable { on_disconnect(conn); });
    connection_map_.insert(std::make_pair(connection_id, conn));
    if (delegate_) {
      delegate_->OnConnect(connection_id);
    }
    VLOG(1) << "Connection (" << factory_.Name() << ") "
            << connection_id << " with " << conn->peer_endpoint()
            << " connected";
    conn->start();
  }

  void on_disconnect(scoped_refptr<ConnectionType> conn) {
    int connection_id = conn->connection_id();
    VLOG(1) << "Connection (" << factory_.Name() << ") "
            << connection_id << " disconnected (has ref "
            << std::boolalpha << conn->HasAtLeastOneRef()
            << std::noboolalpha << ")";
    auto iter = connection_map_.find(connection_id);
    if (iter != connection_map_.end()) {
      connection_map_.erase(iter);
    }
    if (delegate_) {
      delegate_->OnDisconnect(connection_id);
    }
  }

  void setup_ssl_ctx(asio::error_code &ec) {
    load_ca_to_ssl_ctx(ssl_ctx_.native_handle());

    ssl_ctx_.set_options(asio::ssl::context::default_workarounds |
                         asio::ssl::context::no_tlsv1 |
                         asio::ssl::context::no_tlsv1_1, ec);
    if (ec) {
      return;
    }

    ssl_ctx_.set_verify_mode(asio::ssl::verify_peer, ec);
    if (ec) {
      return;
    }

    // Load Certificate Chain Files
    std::string certificate_chain_file = absl::GetFlag(FLAGS_certificate_chain_file);
    std::string private_key_file = absl::GetFlag(FLAGS_private_key_file);
    if (!private_key_file.empty()) {
      ssl_ctx_.set_password_callback([](size_t max_length,
                                        asio::ssl::context::password_purpose purpose) {
        return absl::GetFlag(FLAGS_private_key_password);
      });
      ssl_ctx_.use_certificate_chain_file(certificate_chain_file, ec);
      if (ec) {
        return;
      }
      VLOG(1) << "Using certificate file: " << certificate_chain_file;
      ssl_ctx_.use_private_key_file(private_key_file, asio::ssl::context::pem, ec);
      if (ec) {
        return;
      }
      VLOG(1) << "Using private key file: " << private_key_file;
    }

    // Load Certificates (if set)
    if (!private_key_.empty()) {
      ssl_ctx_.use_certificate_chain(asio::const_buffer(certificate_.data(),
                                                        certificate_.size()),
                                     ec);
      if (ec) {
        return;
      }
      VLOG(1) << "Using certificate (in-memory)";
      ssl_ctx_.use_private_key(asio::const_buffer(private_key_.data(),
                                                  private_key_.size()),
                               asio::ssl::context::pem,
                               ec);
      if (ec) {
        return;
      }
      VLOG(1) << "Using privated key (in-memory)";
    }
    SSL_CTX_set_early_data_enabled(ssl_ctx_.native_handle(),
                                   absl::GetFlag(FLAGS_tls13_early_data));
    // TODO: implement these SSL options
    // SSLServerContextImpl::Init
    // SSL_CTX_set_strict_cipher_list
    // SSL_CTX_set_ocsp_response
    // SSL_CTX_set_signed_cert_timestamp_list
  }

  struct alpn_ctx_t {
    ContentServer<T> *server;
    int connection_id;
  };

  void setup_ssl_ctx_alpn_cb() {
    SSL_CTX *ctx = ssl_ctx_.native_handle();
    alpn_ctx_t *alpn_ctx = new alpn_ctx_t({this, next_connection_id_});
    SSL_CTX_set_alpn_select_cb(ctx, &ContentServer::on_alpn_select, alpn_ctx);
    VLOG(1) << "Alpn support (server) enabled for connection " << next_connection_id_;
  }

  static int on_alpn_select(SSL *ssl,
                            const unsigned char **out,
                            unsigned char *outlen,
                            const unsigned char *in,
                            unsigned int inlen,
                            void *arg) {
    std::unique_ptr<alpn_ctx_t> alpn_ctx(reinterpret_cast<alpn_ctx_t*>(arg));
    auto server = alpn_ctx->server;
    int connection_id = alpn_ctx->connection_id;
    while (inlen) {
      if (in[0] + 1u > inlen) {
        goto err;
      }
      auto alpn = std::string(reinterpret_cast<const char*>(in + 1), in[0]);
      if (!server->https_fallback_ && alpn == "h2") {
        VLOG(1) << "Connection (" << server->factory_.Name() << ") "
          << connection_id << " Alpn support (server) chosen: " << alpn;
        server->set_https_fallback(connection_id, false);
        *out = in + 1;
        *outlen = in[0];
        return SSL_TLSEXT_ERR_OK;
      }
      if (alpn == "http/1.1") {
        VLOG(1) << "Connection (" << server->factory_.Name() << ") "
          << connection_id << " Alpn support (server) chosen: " << alpn;
        server->set_https_fallback(connection_id, true);
        *out = in + 1;
        *outlen = in[0];
        return SSL_TLSEXT_ERR_OK;
      }
      inlen -= 1u + in[0];
      in += 1u + in[0];
    }

  err:
    VLOG(1) << "Connection (" << server->factory_.Name() << ") "
      << connection_id << " Alpn support (server) fatal error";
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  void set_https_fallback(int connection_id, bool https_fallback) {
    auto iter = connection_map_.find(connection_id);
    if (iter != connection_map_.end()) {
      iter->second->set_https_fallback(https_fallback);
    } else {
      VLOG(1) << "Connection (" << factory_.Name() << ") "
        << connection_id << " Set Https Fallback fatal error:"
        << " invalid connection id";
    }
  }

  void setup_upstream_ssl_ctx(asio::error_code &ec) {
    load_ca_to_ssl_ctx(upstream_ssl_ctx_.native_handle());
    upstream_ssl_ctx_.set_options(asio::ssl::context::default_workarounds |
                                  asio::ssl::context::no_tlsv1 |
                                  asio::ssl::context::no_tlsv1_1, ec);
    if (ec) {
      return;
    }

    if (absl::GetFlag(FLAGS_insecure_mode)) {
      upstream_ssl_ctx_.set_verify_mode(asio::ssl::verify_none, ec);
    } else {
      upstream_ssl_ctx_.set_verify_mode(asio::ssl::verify_peer, ec);
      SSL_CTX_set_reverify_on_resume(ssl_ctx_.native_handle(), 1);
    }
    if (ec) {
      return;
    }

    std::string certificate_chain_file = absl::GetFlag(FLAGS_certificate_chain_file);
    if (!certificate_chain_file.empty()) {
      upstream_ssl_ctx_.use_certificate_chain_file(certificate_chain_file, ec);

      if (ec) {
        return;
      }
      VLOG(1) << "Using upstream certificate file: " << certificate_chain_file;
    }
    const auto &cert = upstream_certificate_;
    if (!cert.empty()) {
      upstream_ssl_ctx_.add_certificate_authority(asio::const_buffer(cert.data(), cert.size()), ec);
      if (ec) {
        return;
      }
      VLOG(1) << "Using upstream certificate (in-memory)";
    }

    SSL_CTX *ctx = upstream_ssl_ctx_.native_handle();
    int ret;
    std::vector<unsigned char> alpn_vec = {
      2, 'h', '2',
      8, 'h', 't', 't', 'p', '/', '1', '.', '1'
    };
    if (upstream_https_fallback_) {
      alpn_vec = {
        8, 'h', 't', 't', 'p', '/', '1', '.', '1'
      };
    }
    ret = SSL_CTX_set_alpn_protos(ctx, alpn_vec.data(), alpn_vec.size());
    static_cast<void>(ret);
    DCHECK_EQ(ret, 0);
    if (ret) {
      ec = asio::error::access_denied;
      return;
    }
    VLOG(1) << "Alpn support (client) enabled";
  }

 private:
  asio::io_context &io_context_;
  /// stopping the io_context from running out of work
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;

  std::string remote_host_name_;
  uint16_t remote_port_;

  bool upstream_https_fallback_;
  bool https_fallback_;
  bool enable_upstream_tls_;
  bool enable_tls_;
  std::string upstream_certificate_;
  asio::ssl::context upstream_ssl_ctx_;

  std::string certificate_;
  std::string private_key_;
  asio::ssl::context ssl_ctx_;

  ContentServer::Delegate *delegate_;

  struct ListenCtx {
    asio::ip::tcp::endpoint endpoint;
    asio::ip::tcp::endpoint peer_endpoint;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
  };
  std::array<ListenCtx, MAX_LISTEN_ADDRESSES> listen_ctxs_;
  int next_listen_ctx_ = 0;

  absl::flat_hash_map<int, scoped_refptr<ConnectionType>> connection_map_;

  int next_connection_id_ = 1;

  T factory_;
};

#endif  // H_CONTENT_SERVER
