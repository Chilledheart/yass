// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_STREAM
#define H_STREAM

#include <chrono>
#include <deque>

#include "channel.hpp"
#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "network.hpp"
#include "protocol.hpp"

/// the class to describe the traffic between given node (endpoint)
class stream {
  using io_handle_t = std::function<void(asio::error_code, std::size_t)>;
  using handle_t = std::function<void(asio::error_code)>;
 public:
  /// construct a stream object with ss protocol
  ///
  /// \param io_context the io context associated with the service
  /// \param endpoint the endpoint of the service socket
  /// \param channel the underlying data channel used in stream
  stream(asio::io_context& io_context,
         asio::ip::tcp::endpoint endpoint,
         Channel* channel,
         bool enable_ssl = false)
      : endpoint_(endpoint),
        socket_(io_context),
        connect_timer_(io_context),
        enable_ssl_(enable_ssl),
        ssl_ctx_(asio::ssl::context::tls_client),
        ssl_socket_(socket_, ssl_ctx_),
        channel_(channel) {
    assert(channel && "channel must defined to use with stream");
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

  ~stream() {
    close();
  }

  void setup_ssl() {
    load_ca_to_ssl_ctx(ssl_ctx_);
    ssl_ctx_.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_tlsv1_1);
    ssl_socket_.set_verify_mode(asio::ssl::verify_peer);

#if 0
    ssl_ctx_.add_certificate_authority(asio::const_buffer(data, len), ec);
#endif

    SSL_CTX *ctx = ssl_ctx_.native_handle();
    unsigned char alpn_vec[] = {
      2, 'h', '2',
    };
    int ret = SSL_CTX_set_alpn_protos(ctx, alpn_vec, sizeof(alpn_vec));
    static_cast<void>(ret);
    DCHECK_EQ(ret, 0);
  }

  void connect() {
    Channel* channel = channel_;
    asio::error_code ec;
    socket_.open(endpoint_.protocol(), ec);
    if (ec) {
      closed_ = true;
      channel->disconnected(ec);
      return;
    }
    SetTCPFastOpenConnect(socket_.native_handle(), ec);
    socket_.native_non_blocking(true, ec);
    socket_.non_blocking(true, ec);
    connect_timer_.expires_from_now(
        std::chrono::milliseconds(absl::GetFlag(FLAGS_connect_timeout)));
    connect_timer_.async_wait([this, channel](asio::error_code ec) {
      on_connect_expired(channel, ec);
    });
    socket_.async_connect(endpoint_, [this, channel](asio::error_code ec) {
      if (enable_ssl_ && !ec) {
        ssl_socket_.async_handshake(asio::ssl::stream_base::client,
                                    [this, channel](asio::error_code ec) {
          on_connect(channel, ec);
        });
        return;
      }
      on_connect(channel, ec);
    });
  }

  bool connected() const { return connected_; }

  bool eof() const { return eof_; }

  void disable_read() { read_enabled_ = false; }

  void enable_read(std::function<void()> callback) {
    if (!read_enabled_) {
      read_enabled_ = true;
      if (!read_inprogress_) {
        start_read(callback);
      }
    }
  }

  void start_read(std::function<void()> callback) {
    DCHECK(read_enabled_);
    DCHECK(!read_inprogress_);
    Channel* channel = channel_;
    read_inprogress_ = true;

    s_async_read_some_([this, channel, callback](asio::error_code ec,
                                                 std::size_t bytes_transferred) {
          // Cancelled, safe to ignore
          if (ec == asio::error::operation_aborted) {
            callback();
            return;
          }
          read_inprogress_ = false;
          if (ec) {
            on_read(channel, nullptr, ec, bytes_transferred, callback);
            return;
          }
          if (!read_enabled_) {
            callback();
            return;
          }
          std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
          buf->reserve(0, SOCKET_BUF_SIZE);
          if (!ec) {
            do {
              bytes_transferred = s_read_some_(buf, ec);
              if (ec == asio::error::interrupted) {
                continue;
              }
            } while(false);
          }
          if (ec == asio::error::try_again || ec == asio::error::would_block) {
            start_read(callback);
            return;
          }
          on_read(channel, buf, ec, bytes_transferred, callback);
        });
  }

  size_t read_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    size_t read = s_read_some_(buf, ec);
    rbytes_transferred_ += read;
    if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
      on_disconnect(channel_, ec);
    }
    return read;
  }

  /// start write routine
  ///
  /// \param buf the shared buffer used in write routine
  void start_write(std::shared_ptr<IOBuf> buf, std::function<void()> callback) {
    Channel* channel = channel_;
    DCHECK(!write_inprogress_);
    write_inprogress_ = true;
    s_async_write_some_([this, channel, buf, callback](asio::error_code ec,
                                                       size_t /*bytes_transferred*/) {
          write_inprogress_ = false;
          // Cancelled, safe to ignore
          if (ec == asio::error::operation_aborted) {
            callback();
            return;
          }
          if (ec) {
            on_write(channel, nullptr, ec, 0, callback);
            return;
          }

          size_t bytes_transferred;
          do {
            bytes_transferred = s_write_some_(buf, ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
          buf->trimStart(bytes_transferred);

          if (ec == asio::error::try_again || ec == asio::error::would_block) {
            start_write(buf, callback);
            return;
          }
          if (ec) {
            on_write(channel, buf, ec, bytes_transferred, callback);
            return;
          }
          if (!buf->empty()) {
            start_write(buf, callback);
            return;
          }
          on_write(channel, buf, ec, bytes_transferred, callback);
    });
  }

  size_t write_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    size_t written = s_write_some_(buf, ec);
    wbytes_transferred_ += written;
    if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
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
    if (enable_ssl_) {
      // FIXME use async_shutdown correctly
      ssl_socket_.shutdown(ec);
    }
    socket_.close(ec);
    if (ec) {
      VLOG(2) << "close() error: " << ec;
    }
  }

 private:
  void on_connect(Channel* channel,
                  asio::error_code ec) {
    connect_timer_.cancel();
    if (ec) {
      channel->disconnected(ec);
      return;
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
  }

  void on_connect_expired(Channel* channel,
                          asio::error_code ec) {
    // Cancelled, safe to ignore
    if (ec == asio::error::operation_aborted) {
      return;
    }
    VLOG(1) << "connection timed out with endpoint: " << endpoint_;
    eof_ = true;
    if (!ec) {
      ec = asio::error::timed_out;
    }
    channel->disconnected(ec);
  }

  void on_read(Channel* channel,
               std::shared_ptr<IOBuf> buf,
               asio::error_code ec,
               size_t bytes_transferred,
               std::function<void()> callback) {
    rbytes_transferred_ += bytes_transferred;
    if (buf) {
      buf->append(bytes_transferred);
    }

    if (ec || bytes_transferred == 0) {
      eof_ = true;
    }

    if (!connected_) {
      callback();
      return;
    }

    if (bytes_transferred) {
      channel->received(buf);
      if (read_enabled_) {
        start_read(callback);
        return;
      }
    }

    if (ec) {
      DCHECK(!bytes_transferred) << "data receiving failed with data "
        << endpoint_ << " due to " << ec;
      on_disconnect(channel, ec);
    }
    callback();
  }

  void on_write(Channel* channel,
                std::shared_ptr<IOBuf> buf,
                asio::error_code ec,
                size_t bytes_transferred,
                std::function<void()> callback) {
    wbytes_transferred_ += bytes_transferred;

    if (ec || bytes_transferred == 0) {
      eof_ = true;
    }

    if (!connected_) {
      callback();
      return;
    }

    if (bytes_transferred) {
      DCHECK_EQ(buf->length(), 0u);
      channel->sent(buf, bytes_transferred);
    }

    if (ec) {
      DCHECK(!bytes_transferred) << "data sending failed with data "
        << endpoint_ << " due to " << ec;
      on_disconnect(channel, ec);
    }
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
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;

  const bool enable_ssl_;
  asio::ssl::context ssl_ctx_;
  asio::ssl::stream<asio::ip::tcp::socket&> ssl_socket_;

  Channel* channel_;
  bool connected_ = false;
  bool eof_ = false;
  bool closed_ = false;

  bool read_enabled_ = true;
  bool read_inprogress_ = false;
  bool write_inprogress_ = false;

  std::function<void(io_handle_t)> s_async_read_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_read_some_;
  std::function<void(io_handle_t)> s_async_write_some_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_write_some_;
  std::function<void(handle_t)> s_async_shutdown_;
  std::function<void(asio::error_code&)> s_shutdown_;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

#endif  // H_STREAM
