// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef H_NET_SSL_STREAM
#define H_NET_SSL_STREAM

#include "net/stream.hpp"

namespace net {

/// the class to describe the ssl traffic between given node (endpoint)
class ssl_stream : public stream {
 public:
  /// construct a ssl_stream object
  template <typename... Args>
  static scoped_refptr<ssl_stream> create(Args&&... args) {
    return MakeRefCounted<ssl_stream>(std::forward<Args>(args)...);
  }

  /// construct a ssl stream object with ss protocol
  ///
  /// \param ssl_socket_data_index the ssl client data index
  /// \param io_context the io context associated with the service
  /// \param host_ips the ip addresses used with endpoint
  /// \param host_sni the sni name used with endpoint
  /// \param port the sni port used with endpoint
  /// \param channel the underlying data channel used in stream
  /// \param https_fallback the data channel falls back to https (alpn)
  /// \param ssl_ctx the ssl context object for tls data transfer
  ssl_stream(int ssl_socket_data_index,
             asio::io_context& io_context,
             const std::string& host_ips,
             const std::string& host_sni,
             uint16_t port,
             Channel* channel,
             bool https_fallback,
             SSL_CTX* ssl_ctx)
      : stream(io_context, host_ips, host_sni, port, channel),
        https_fallback_(https_fallback),
        enable_tls_(true),
        ssl_socket_(SSLSocket::Create(ssl_socket_data_index,
                                      &io_context,
                                      &socket_,
                                      ssl_ctx,
                                      https_fallback,
                                      host_sni)) {}

  ~ssl_stream() override {}

  bool https_fallback() const override { return https_fallback_; }

 protected:
  void s_wait_read(handle_t&& cb) override { ssl_socket_->WaitRead(std::move(cb)); }

  size_t s_read_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) override { return ssl_socket_->Read(buf, ec); }

  void s_wait_write(handle_t&& cb) override { ssl_socket_->WaitWrite(std::move(cb)); }

  size_t s_write_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) override { return ssl_socket_->Write(buf, ec); }

  void s_async_shutdown(handle_t&& cb) override { ssl_socket_->Shutdown(std::move(cb)); }

  void s_shutdown(asio::error_code& ec) override {
    ec = asio::error_code();
    ssl_socket_->Shutdown([](asio::error_code ec) {}, true);
  }

  void s_close(asio::error_code& ec) override {
    ec = asio::error_code();
    ssl_socket_->Disconnect();
  }

  void on_async_connected(Channel* channel, asio::error_code ec) override {
    if (ec) {
      stream::on_async_connected(channel, ec);
      return;
    }
    scoped_refptr<stream> self(this);
    ssl_socket_->Connect([this, channel, self](int rv) {
      if (closed_) {
        DCHECK(!user_connect_callback_);
        return;
      }
      asio::error_code ec;
      if (rv < 0) {
        ec = asio::error::connection_refused;
        on_async_connected(channel, ec);
        return;
      }

      std::string alpn = ssl_socket_->negotiated_protocol();
      if (!alpn.empty()) {
        VLOG(2) << "Alpn selected (client): " << alpn;
      }
      https_fallback_ |= alpn == "http/1.1";
      if (https_fallback_) {
        VLOG(2) << "Alpn fallback to https protocol (client)";
      }

      stream::on_async_connected(channel, ec);

      scoped_refptr<stream> self(this);
      // Also queue a ConfirmHandshake. It should also be blocked on ServerHello.
      auto cb = [this, self, channel](int rv) {
        if (closed_) {
          DCHECK(!user_connect_callback_);
          return;
        }
        asio::error_code ec;
        if (rv < 0) {
          ec = asio::error::connection_refused;
          channel->disconnected(ec);
        }
      };
      ssl_socket_->ConfirmHandshake(cb);
    });
  }

 private:
  bool https_fallback_;
  const bool enable_tls_;
  scoped_refptr<SSLSocket> ssl_socket_;
};

}  // namespace net

#endif  // H_NET_SSL_STREAM
