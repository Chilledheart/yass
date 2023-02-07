// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_STREAM
#define H_STREAM

#include <chrono>
#include <deque>

#include "channel.hpp"
#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/c-ares.hpp"
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "network.hpp"
#include "protocol.hpp"

#include <absl/strings/str_cat.h>
#include <openssl/bio.h>

/// the class to describe the traffic between given node (endpoint)
class stream {
  using io_handle_t = std::function<void(asio::error_code, std::size_t)>;
  using handle_t = std::function<void(asio::error_code)>;
 public:
  /// construct a stream object with ss protocol
  ///
  /// \param io_context the io context associated with the service
  /// \param host_name the sni name used with endpoint
  /// \param port the sni port used with endpoint
  /// \param channel the underlying data channel used in stream
  /// \param https_fallback the data channel falls back to https (alpn)
  /// \param enable_tls the underlying data channel is using tls
  /// \param ssl_ctx the ssl context object for tls data transfer
  stream(asio::io_context& io_context,
         const std::string& host_name,
         uint16_t port,
         Channel* channel,
         bool https_fallback,
         bool enable_tls,
         asio::ssl::context *ssl_ctx)
      : resolver_(CAresResolver::Create(io_context)),
        host_name_(host_name),
        port_(port),
        socket_(io_context),
        connect_timer_(io_context),
        https_fallback_(https_fallback),
        enable_tls_(enable_tls),
        ssl_socket_(socket_, *ssl_ctx),
        channel_(channel) {
    assert(channel && "channel must defined to use with stream");
    int ret = resolver_->Init(1000, 5);
    CHECK_EQ(ret, 0) << "c-ares initialize failure";
    static_cast<void>(ret);
    if (enable_tls) {
      setup_ssl();
      s_async_read_some_ = [this](handle_t cb) {
        socket_.async_wait(asio::ip::tcp::socket::wait_read, cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_.read_some(mutable_buffer(*buf), ec);
      };
      s_async_write_some_ = [this](handle_t cb) {
        socket_.async_wait(asio::ip::tcp::socket::wait_write, cb);
      };
      s_write_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_.write_some(const_buffer(*buf), ec);
      };
      s_async_shutdown_ = [this](handle_t cb) {
        ssl_socket_.async_shutdown(cb);
      };
      s_shutdown_ = [this](asio::error_code &ec) {
        ssl_socket_.shutdown(ec);
      };
    } else {
      s_async_read_some_ = [this](handle_t cb) {
        socket_.async_wait(asio::ip::tcp::socket::wait_read, cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return socket_.read_some(mutable_buffer(*buf), ec);
      };
      s_async_write_some_ = [this](handle_t cb) {
        socket_.async_wait(asio::ip::tcp::socket::wait_write, cb);
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
        socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
      };
    }
  }

  ~stream() {
    close();
  }

  void setup_ssl() {
    SSL* ssl = ssl_socket_.native_handle();
    // TODO: implement these SSL options
    // SSLClientSocketImpl::Init
    // SSL_CTX_set_strict_cipher_list
    if (!host_name_.empty()) {
      asio::error_code ec;
      asio::ip::make_address(host_name_.c_str(), ec);
      bool host_is_ip_address = !ec;
      if (!host_is_ip_address) {
        int ret = ::SSL_set_tlsext_host_name(ssl, host_name_.c_str());
        CHECK_EQ(ret, 1) << "SSL_set_tlsext_host_name failure";
      }
    }

    // Whether early data is enabled on this connection.
    ::SSL_set_early_data_enabled(ssl, absl::GetFlag(FLAGS_tls13_early_return));

    // ALPS TLS extension is enabled and corresponding data is sent to client if
    // client also enabled ALPS, for each NextProto in |application_settings|.
    // Data might be empty.
    const char* proto_string = https_fallback_ ? "http/1.1" : "h2";
    std::vector<uint8_t> data;
    ::SSL_add_application_settings(ssl,
                                   reinterpret_cast<const uint8_t*>(proto_string),
                                   strlen(proto_string), data.data(), data.size());
    ::SSL_enable_signed_cert_timestamps(ssl);
    ::SSL_enable_ocsp_stapling(ssl);

    // Configure BoringSSL to allow renegotiations. Once the initial handshake
    // completes, if renegotiations are not allowed, the default reject value will
    // be restored. This is done in this order to permit a BoringSSL
    // optimization. See https://crbug.com/boringssl/123. Use
    // ssl_renegotiate_explicit rather than ssl_renegotiate_freely so DoPeek()
    // does not trigger renegotiations.
    ::SSL_set_renegotiate_mode(ssl, ssl_renegotiate_explicit);

    ::SSL_set_shed_handshake_config(ssl, 1);

    // If false, disables TLS Encrypted ClientHello (ECH). If true, the feature
    // may be enabled or disabled, depending on feature flags.
    ::SSL_set_enable_ech_grease(ssl, 0);
    ssl_socket_.set_verify_mode(asio::ssl::verify_peer);
  }

  void connect(std::function<void()> callback) {
    Channel* channel = channel_;
    DCHECK_EQ(closed_, false);

    asio::error_code ec;
    auto addr = asio::ip::make_address(host_name_.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (host_is_ip_address) {
      endpoint_ = asio::ip::tcp::endpoint(addr, port_);
      VLOG(2) << "resolved ip-like address: " << endpoint_;
      on_resolve(callback, channel);
      return;
    }

    resolver_->AsyncResolve(host_name_, std::to_string(port_),
      [this, channel, callback](const asio::error_code& ec,
                                asio::ip::tcp::resolver::results_type results) {
      if (closed_) {
        callback();
        return;
      }
      if (ec) {
        on_connect(callback, channel, ec);
        return;
      }

      /// FIXME TBD
      endpoint_ = results->endpoint();
      VLOG(2) << "resolved address from: " << domain() << " to: " << endpoint_;
      on_resolve(callback, channel);
    });
  }

  std::string domain() {
    return absl::StrCat(host_name_, ":", std::to_string(port_));
  }

  bool connected() const { return connected_; }

  bool eof() const { return eof_; }

  void disable_read() { read_enabled_ = false; }

  void enable_read(std::function<void()> callback, int capacity = SOCKET_BUF_SIZE) {
    if (!read_enabled_) {
      read_enabled_ = true;
      if (!read_inprogress_) {
        start_read(callback);
      }
    }
  }

  bool do_peek() {
    if (enable_tls_) {
      char byte;
      auto ssl = ssl_socket_.native_handle();
      int rv = SSL_peek(ssl, &byte, 1);
      int ssl_err = SSL_get_error(ssl, rv);
      if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
        return true;
      }
    }
    asio::error_code ec;
    if (socket_.available(ec)) {
      return true;
    }
    return false;
  }

  bool read_inprogress() const {
    return read_inprogress_;
  }

  /// start read routine
  ///
  void start_read(std::function<void()> callback) {
    DCHECK(read_enabled_);
    DCHECK(!read_inprogress_);
    Channel* channel = channel_;

    if (!connected_ || closed_) {
      callback();
      return;
    }
    if (do_peek()) {
      channel_->received();
      if (read_enabled_) {
        start_read(callback);
      }
      return;
    }

    read_inprogress_ = true;
    s_async_read_some_([this, channel, callback] (asio::error_code ec) {
        read_inprogress_ = false;
        // Cancelled, safe to ignore
        if (ec == asio::error::operation_aborted) {
          callback();
          return;
        }
        if (!connected_ || closed_) {
          callback();
          return;
        }
        if (ec) {
          on_disconnect(channel, ec);
          callback();
          return;
        }
        if (!read_enabled_) {
          callback();
          return;
        }
        channel_->received();
        if (read_enabled_) {
          start_read(callback);
          return;
        }
        callback();
    });
  }

  size_t read_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    DCHECK(!closed_ && "I/O on closed upstream connection");
    size_t read = s_read_some_(buf, ec);
    rbytes_transferred_ += read;
    if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
      eof_ = true;
      on_disconnect(channel_, ec);
    }
    return read;
  }

  bool write_inprogress() const {
    return write_inprogress_;
  }

  /// start write routine
  ///
  void start_write(std::function<void()> callback) {
    Channel* channel = channel_;
    DCHECK(!write_inprogress_);
    write_inprogress_ = true;
    s_async_write_some_([this, channel, callback](asio::error_code ec) {
        write_inprogress_ = false;
        // Cancelled, safe to ignore
        if (ec == asio::error::operation_aborted) {
          callback();
          return;
        }
        if (!connected_ || closed_) {
          callback();
          return;
        }
        if (ec) {
          on_disconnect(channel, ec);
          callback();
          return;
        }

        channel->sent();
        callback();
    });
  }

  size_t write_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    DCHECK(!closed_ && "I/O on closed upstream connection");
    size_t written = s_write_some_(buf, ec);
    wbytes_transferred_ += written;
    if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
      eof_ = true;
      on_disconnect(channel_, ec);
    }
    return written;
  }

  void close() {
    if (closed_) {
      return;
    }
    eof_ = true;
    closed_ = true;

    asio::error_code ec;
    if (enable_tls_) {
      socket_.native_non_blocking(false, ec);
      socket_.non_blocking(false, ec);
      ssl_socket_.shutdown(ec);
      if (ec) {
        VLOG(2) << "shutdown() error: " << ec;
      }
    }
    socket_.close(ec);
    if (ec) {
      VLOG(2) << "close() error: " << ec;
    }
    connect_timer_.cancel();
    resolver_->Cancel();
  }

  bool https_fallback() const { return https_fallback_; }

 private:
  void on_resolve(std::function<void()> callback, Channel* channel) {
    asio::error_code ec;
    socket_.open(endpoint_.protocol(), ec);
    if (ec) {
      closed_ = true;
      channel->disconnected(ec);
      callback();
      return;
    }
    SetTCPFastOpenConnect(socket_.native_handle(), ec);
    socket_.native_non_blocking(true, ec);
    socket_.non_blocking(true, ec);
    connect_timer_.expires_from_now(
        std::chrono::milliseconds(absl::GetFlag(FLAGS_connect_timeout)));
    connect_timer_.async_wait(
      [this, channel, callback](asio::error_code ec) {
      on_connect_expired(callback, channel, ec);
    });
    socket_.async_connect(endpoint_,
      [this, channel, callback](asio::error_code ec) {
      if (closed_) {
        callback();
        return;
      }
      if (enable_tls_ && !ec) {
        ssl_socket_.async_handshake(asio::ssl::stream_base::client,
                                    [this, channel, callback](asio::error_code ec) {
          if (closed_) {
            callback();
            return;
          }
          on_connect(callback, channel, ec);
        });
        return;
      }
      on_connect(callback, channel, ec);
    });
  }

  void on_connect(std::function<void()> callback, Channel* channel, asio::error_code ec) {
    connect_timer_.cancel();
    if (ec) {
      DCHECK_NE(ec, asio::error::operation_aborted);
      channel->disconnected(ec);
      callback();
      return;
    }
    if (enable_tls_) {
      SSL* ssl = ssl_socket_.native_handle();
      const unsigned char* out;
      unsigned int outlen;
      SSL_get0_alpn_selected(ssl, &out, &outlen);
      std::string alpn = std::string(reinterpret_cast<const char*>(out), outlen);
      VLOG(2) << "Alpn selected (client): " << alpn;
      https_fallback_ |= alpn == "http/1.1";
      if (https_fallback_) {
        VLOG(2) << "Alpn fallback to https protocol (client)";
      }
    }
    connected_ = true;
    SetTCPCongestion(socket_.native_handle(), ec);
    SetTCPConnectionTimeout(socket_.native_handle(), ec);
    SetTCPUserTimeout(socket_.native_handle(), ec);
    SetTCPKeepAlive(socket_.native_handle(), ec);
    SetSocketLinger(&socket_, ec);
    SetSocketSndBuffer(&socket_, ec);
    SetSocketRcvBuffer(&socket_, ec);
    channel->connected();
    callback();
  }

  void on_connect_expired(std::function<void()> callback,
                          Channel* channel,
                          asio::error_code ec) {
    // Cancelled, safe to ignore
    if (ec == asio::error::operation_aborted) {
      callback();
      return;
    }
    VLOG(1) << "connection timed out with endpoint: " << endpoint_;
    eof_ = true;
    if (!ec) {
      ec = asio::error::timed_out;
    }
    channel->disconnected(ec);
    callback();
  }

  void on_disconnect(Channel* channel,
                     asio::error_code ec) {
    if (ec) {
      VLOG(2) << "data transfer failed with " << endpoint_ << " due to "
              << ec << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
    } else {
      VLOG(2) << "data transfer closed with: " << endpoint_ << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
    }
    connected_ = false;
    channel->disconnected(ec);
  }

 public:
  size_t rbytes_transferred() const { return rbytes_transferred_; }
  size_t wbytes_transferred() const { return wbytes_transferred_; }

 private:
  /// used to resolve local and remote endpoint
  scoped_refptr<CAresResolver> resolver_;

  std::string host_name_;
  uint16_t port_;
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;

  bool https_fallback_;
  const bool enable_tls_;
  asio::ssl::stream<asio::ip::tcp::socket&> ssl_socket_;

  Channel* channel_;
  bool connected_ = false;
  bool eof_ = false;
  bool closed_ = false;

  bool read_enabled_ = true;
  bool read_inprogress_ = false;
  bool write_inprogress_ = false;

  std::function<void(handle_t)> s_async_read_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_read_some_;
  std::function<void(handle_t)> s_async_write_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_write_some_;
  std::function<void(handle_t)> s_async_shutdown_;
  std::function<void(asio::error_code&)> s_shutdown_;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

#endif  // H_STREAM
