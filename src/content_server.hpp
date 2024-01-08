// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

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
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "network.hpp"
#include "net/x509_util.hpp"

#define MAX_LISTEN_ADDRESSES 30

/// An interface used to provide service
template <typename T>
class ContentServer {
 public:
  using ConnectionType = typename T::ConnectionType;
  using tlsext_ctx_t = typename ConnectionType::tlsext_ctx_t;
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnConnect(int connection_id) = 0;
    virtual void OnDisconnect(int connection_id) = 0;
  };

 public:
  explicit ContentServer(asio::io_context &io_context,
                         const std::string& remote_host_ips = {},
                         const std::string& remote_host_sni = {},
                         uint16_t remote_port = {},
                         const std::string& upstream_certificate = {},
                         const std::string& certificate = {},
                         const std::string& private_key = {},
                         ContentServer::Delegate *delegate = nullptr)
    : io_context_(io_context),
      work_guard_(std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor())),
      remote_host_ips_(remote_host_ips),
      remote_host_sni_(remote_host_sni),
      remote_port_(remote_port),
      upstream_https_fallback_(absl::GetFlag(FLAGS_method).method == CRYPTO_HTTPS),
      https_fallback_(absl::GetFlag(FLAGS_method).method == CRYPTO_HTTPS),
      enable_upstream_tls_(
          absl::GetFlag(FLAGS_method).method == CRYPTO_HTTPS ||
          absl::GetFlag(FLAGS_method).method == CRYPTO_HTTP2),
      enable_tls_(
          absl::GetFlag(FLAGS_method).method == CRYPTO_HTTPS ||
          absl::GetFlag(FLAGS_method).method == CRYPTO_HTTP2),
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
              const std::string& server_name,
              int backlog,
              asio::error_code& ec) {
    if (next_listen_ctx_ >= MAX_LISTEN_ADDRESSES) {
      ec = asio::error::already_started;
      return;
    }
    ListenCtx& ctx = listen_ctxs_[next_listen_ctx_];
    ctx.server_name = server_name;
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
  void shutdown() {
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
      if (connection_map_.empty()) {
        LOG(WARNING) << "No more connections alive... ready to stop";
        work_guard_.reset();
        in_shutdown_ = false;
      } else {
        LOG(WARNING) << "Waiting for remaining connects: " << connection_map_.size();
        in_shutdown_ = true;
      }
    });
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
      opened_connections_ = 0;
      for (auto [conn_id, conn] : connection_map) {
        VLOG(1) << "Connections (" << factory_.Name() << ")"
                << " closing Connection: " << conn_id;
        conn->close();
      }

      work_guard_.reset();
    });
  }

  size_t num_of_connections() const {
    return opened_connections_;
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
          // cancelled
          if (ec == asio::error::operation_aborted) {
            return;
          }
          if (ec) {
            LOG(WARNING) << "Acceptor (" << factory_.Name() << ")"
                         << " failed to accept more due to: " << ec;
            work_guard_.reset();
            return;
          }
          tlsext_ctx_t* tlsext_ctx = nullptr;
          if (enable_tls_) {
            tlsext_ctx = new tlsext_ctx_t{this, next_connection_id_, listen_ctx_num};
            setup_ssl_ctx_alpn_cb(tlsext_ctx);
            setup_ssl_ctx_tlsext_cb(tlsext_ctx);
          }
          scoped_refptr<ConnectionType> conn = factory_.Create(
            io_context_, remote_host_ips_, remote_host_sni_, remote_port_,
            upstream_https_fallback_, https_fallback_,
            enable_upstream_tls_, enable_tls_,
            &upstream_ssl_ctx_, &ssl_ctx_);
          on_accept(conn, std::move(socket), listen_ctx_num, tlsext_ctx);
          if (in_shutdown_) {
            return;
          }
          if (connection_map_.size() >= absl::GetFlag(FLAGS_worker_connections)) {
            LOG(INFO) << "Disabling accepting new connection: " << listen_ctxs_[listen_ctx_num].endpoint;
            pending_next_listen_ctxes_.push_back(listen_ctx_num);
            return;
          }
          accept(listen_ctx_num);
        });
  }

  void on_accept(scoped_refptr<ConnectionType> conn,
                 asio::ip::tcp::socket &&socket,
                 int listen_ctx_num,
                 tlsext_ctx_t *tlsext_ctx) {
    asio::error_code ec;
    ListenCtx& ctx = listen_ctxs_[listen_ctx_num];

    int connection_id = next_connection_id_++;
    socket.native_non_blocking(true, ec);
    socket.non_blocking(true, ec);
    SetTCPCongestion(socket.native_handle(), ec);
    SetTCPKeepAlive(socket.native_handle(), ec);
    SetSocketTcpNoDelay(&socket, ec);
    conn->on_accept(std::move(socket), ctx.endpoint, ctx.peer_endpoint,
                    connection_id, tlsext_ctx);
    conn->set_disconnect_cb(
        [this, conn]() mutable { on_disconnect(conn); });
    connection_map_.insert(std::make_pair(connection_id, conn));
    ++opened_connections_;
    DCHECK_EQ(connection_map_.size(), opened_connections_);
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
      --opened_connections_;
      DCHECK_EQ(connection_map_.size(), (size_t)opened_connections_);
    }
    if (delegate_) {
      delegate_->OnDisconnect(connection_id);
    }
    // reset guard to quit io loop if in shutdown
    if (in_shutdown_) {
      pending_next_listen_ctxes_.clear();
      if (connection_map_.empty()) {
        LOG(WARNING) << "No more connections alive... ready to stop";
        work_guard_.reset();
        in_shutdown_ = false;
      } else {
        LOG(WARNING) << "Waiting for remaining connects: " << connection_map_.size();
      }
    }
    auto listen_ctxes = std::move(pending_next_listen_ctxes_);
    for (int listen_ctx_num : listen_ctxes) {
      LOG(INFO) << "Resuming accepting new connection: " << listen_ctxs_[listen_ctx_num].endpoint;
      accept(listen_ctx_num);
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

    SSL_CTX_set_session_cache_mode(ssl_ctx_.native_handle(), SSL_SESS_CACHE_SERVER);

    // Load Certificate Chain Files
    if (private_key_.empty()) {
      private_key_ = g_private_key_content;
      certificate_ = g_certificate_chain_content;
    }

    // Load Certificates (if set)
    if (!private_key_.empty()) {
      CHECK(!certificate_.empty()) << "certificate buffer is not provided";
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

    CHECK(SSL_CTX_set_min_proto_version(ssl_ctx_.native_handle(),
                                        TLS1_2_VERSION));
    CHECK(SSL_CTX_set_max_proto_version(ssl_ctx_.native_handle(),
                                        TLS1_3_VERSION));

    // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
    // set everything we care about to an absolute value.
    net::SslSetClearMask options;
    options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);

    SSL_CTX_set_options(ssl_ctx_.native_handle(), options.set_mask);
    SSL_CTX_clear_options(ssl_ctx_.native_handle(), options.clear_mask);

    // Same as above, this time for the SSL mode.
    net::SslSetClearMask mode;

    mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);

    SSL_CTX_set_mode(ssl_ctx_.native_handle(), mode.set_mask);
    SSL_CTX_clear_mode(ssl_ctx_.native_handle(), mode.clear_mask);

    // Use BoringSSL defaults, but disable 3DES and HMAC-SHA1 ciphers in ECDSA.
    // These are the remaining CBC-mode ECDSA ciphers.
    std::string command("ALL:!aPSK:!ECDSA+SHA1:!3DES");

#if 0
    // SSLPrivateKey only supports ECDHE-based ciphers because it lacks decrypt.
    if (ssl_server_config_.require_ecdhe || (!pkey_ && private_key_))
      command.append(":!kRSA");

    // Remove any disabled ciphers.
    for (uint16_t id : ssl_server_config_.disabled_cipher_suites) {
      const SSL_CIPHER* cipher = SSL_get_cipher_by_value(id);
      if (cipher) {
        command.append(":!");
        command.append(SSL_CIPHER_get_name(cipher));
      }
    }
#endif

    CHECK(SSL_CTX_set_strict_cipher_list(ssl_ctx_.native_handle(), command.c_str()));

    // TODO: implement these SSL options
    // SSL_CTX_set_ocsp_response
    // SSL_CTX_set_signed_cert_timestamp_list
    // SSL_CTX_set1_ech_keys

    // Deduplicate all certificates minted from the SSL_CTX in memory.
    SSL_CTX_set0_buffer_pool(ssl_ctx_.native_handle(), x509_util::GetBufferPool());
  }

  void setup_ssl_ctx_alpn_cb(tlsext_ctx_t *tlsext_ctx) {
    SSL_CTX *ctx = ssl_ctx_.native_handle();
    SSL_CTX_set_alpn_select_cb(ctx, &ContentServer::on_alpn_select, tlsext_ctx);
    VLOG(1) << "Alpn support (server) enabled for connection " << next_connection_id_;
  }

  static int on_alpn_select(SSL *ssl,
                            const unsigned char **out,
                            unsigned char *outlen,
                            const unsigned char *in,
                            unsigned int inlen,
                            void *arg) {
    auto tlsext_ctx = reinterpret_cast<tlsext_ctx_t*>(arg);
    auto server = reinterpret_cast<ContentServer*>(tlsext_ctx->server);
    int connection_id = tlsext_ctx->connection_id;
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
      LOG(WARNING) << "Unexpected alpn: " << alpn;
      inlen -= 1u + in[0];
      in += 1u + in[0];
    }

  err:
    LOG(WARNING) << "Connection (" << server->factory_.Name() << ") "
      << connection_id << " Alpn support (server) fatal error";
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  void setup_ssl_ctx_tlsext_cb(tlsext_ctx_t *tlsext_ctx) {
    SSL_CTX *ctx = ssl_ctx_.native_handle();
    SSL_CTX_set_tlsext_servername_callback(ctx, &ContentServer::on_tlsext);
    SSL_CTX_set_tlsext_servername_arg(ctx, tlsext_ctx);

    VLOG(1) << "TLSEXT: Servername (server) enabled for connection "
      << next_connection_id_ << " server_name: "
      << listen_ctxs_[tlsext_ctx->listen_ctx_num].server_name;
  }

  static int on_tlsext(SSL *ssl,
                                  int *al,
                                  void *arg) {
    auto tlsext_ctx = reinterpret_cast<tlsext_ctx_t*>(arg);
    auto server = reinterpret_cast<ContentServer*>(tlsext_ctx->server);
    int connection_id = tlsext_ctx->connection_id;
    int listen_ctx_num = tlsext_ctx->listen_ctx_num;
    absl::string_view expected_server_name = server->listen_ctxs_[listen_ctx_num].server_name;

    // SNI must be accessible from the SNI callback.
    const char *server_name_ptr = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    absl::string_view server_name;
    if (server_name_ptr)  {
      server_name = server_name_ptr;
    }
    // Allow once if matched
    if (server_name == expected_server_name) {
      return SSL_TLSEXT_ERR_OK;
    }

    VLOG(1) << "Connection (" << server->factory_.Name() << ") "
      << connection_id << " TLSEXT: Servername mismatch "
      << "(got " << server_name << "; want " << expected_server_name << ").";
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
      SSL_CTX_set_reverify_on_resume(upstream_ssl_ctx_.native_handle(), 1);
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

#if 0
    // Disable the internal session cache. Session caching is handled
    // externally (i.e. by SSLClientSessionCache).
    SSL_CTX_set_session_cache_mode(upstream_ssl_ctx_.native_handle(),
                                   SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL);
    SSL_CTX_sess_set_new_cb(upstream_ssl_ctx_.native_handle(), NewSessionCallback);
#endif

    SSL_CTX_set_timeout(upstream_ssl_ctx_.native_handle(), 1 * 60 * 60 /* one hour */);

    SSL_CTX_set_grease_enabled(upstream_ssl_ctx_.native_handle(), 1);

    // Deduplicate all certificates minted from the SSL_CTX in memory.
    SSL_CTX_set0_buffer_pool(upstream_ssl_ctx_.native_handle(), x509_util::GetBufferPool());
  }

#if 0
 private:
  static int NewSessionCallback(SSL* ssl, SSL_SESSION* session) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->NewSessionCallback(session);
  }
#endif

 private:
  asio::io_context &io_context_;
  /// stopping the io_context from running out of work
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;

  std::string remote_host_ips_;
  std::string remote_host_sni_;
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
    std::string server_name;
    asio::ip::tcp::endpoint endpoint;
    asio::ip::tcp::endpoint peer_endpoint;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
  };
  std::array<ListenCtx, MAX_LISTEN_ADDRESSES> listen_ctxs_;
  int next_listen_ctx_ = 0;
  std::vector<int> pending_next_listen_ctxes_;
  bool in_shutdown_ = false;

  absl::flat_hash_map<int, scoped_refptr<ConnectionType>> connection_map_;

  int next_connection_id_ = 1;
  std::atomic<size_t> opened_connections_ = 0;

  T factory_;
};

#endif  // H_CONTENT_SERVER
