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
#include "core/utils.hpp"
#include "network.hpp"
#include "net/ssl_socket.hpp"
#include "protocol.hpp"

#ifdef HAVE_C_ARES
#include "core/c-ares.hpp"
#endif

#include <absl/strings/str_cat.h>

/// the class to describe the traffic between given node (endpoint)
class stream : public RefCountedThreadSafe<stream> {
  using io_handle_t = std::function<void(asio::error_code, std::size_t)>;
  using handle_t = std::function<void(asio::error_code)>;

 public:
  /// construct a stream object
  template<typename... Args>
  static scoped_refptr<stream> create(Args&&... args) {
    return MakeRefCounted<stream>(std::forward<Args>(args)...);
  }

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
#ifdef HAVE_C_ARES
      : resolver_(CAresResolver::Create(io_context)),
#else
      : resolver_(io_context),
#endif
        host_name_(host_name),
        port_(port),
        io_context_(io_context),
        socket_(io_context),
        connect_timer_(io_context),
        https_fallback_(https_fallback),
        enable_tls_(enable_tls),
        ssl_socket_(enable_tls ? net::SSLSocket::Create(&io_context, &socket_, ssl_ctx->native_handle(), https_fallback, host_name) : nullptr),
        channel_(channel) {
    CHECK(channel && "channel must defined to use with stream");
#ifdef HAVE_C_ARES
    int ret = resolver_->Init(5000);
    CHECK_EQ(ret, 0) << "c-ares initialize failure";
    static_cast<void>(ret);
#endif
    if (enable_tls) {
      s_wait_read_ = [this](handle_t cb) {
        ssl_socket_->WaitRead(cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_->Read(buf, ec);
      };
      s_wait_write = [this](handle_t cb) {
        ssl_socket_->WaitWrite(cb);
      };
      s_write_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return ssl_socket_->Write(buf, ec);
      };
      s_async_shutdown_ = [this](handle_t cb) {
        ssl_socket_->Shutdown(cb);
      };
      s_shutdown_ = [this](asio::error_code &ec) {
        ec = asio::error_code();
        ssl_socket_->Shutdown([](asio::error_code ec){}, true);
      };
    } else {
      s_wait_read_ = [this](handle_t cb) {
        socket_.async_wait(asio::ip::tcp::socket::wait_read, cb);
      };
      s_read_some_ = [this](std::shared_ptr<IOBuf> buf, asio::error_code &ec) -> size_t {
        return socket_.read_some(tail_buffer(*buf), ec);
      };
      s_wait_write = [this](handle_t cb) {
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

  void on_async_connect_callback(asio::error_code ec) {
    if (auto cb = std::move(user_connect_callback_)) {
      cb(ec);
    }
  }

  void async_connect(handle_t callback) {
    Channel* channel = channel_;
    DCHECK_EQ(closed_, false);
    DCHECK(callback);
    user_connect_callback_ = std::move(callback);

    asio::error_code ec;
    auto addr = asio::ip::make_address(host_name_.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (host_is_ip_address) {
      VLOG(2) << "resolved ip-like address: " << domain();
      endpoints_.emplace_back(addr, port_);
      on_try_next_endpoint(channel);
      return;
    }

    scoped_refptr<stream> self(this);
#ifdef HAVE_C_ARES
    resolver_->AsyncResolve(host_name_, std::to_string(port_),
#else
    resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(),
                            host_name_, std::to_string(port_),
#endif
      [this, channel, self](const asio::error_code& ec,
                            asio::ip::tcp::resolver::results_type results) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::operation_aborted)) {
        return;
      }
      if (closed_) {
        DCHECK(!user_connect_callback_);
        return;
      }
      if (ec) {
        on_async_connected(channel, ec);
        return;
      }
      for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
        VLOG(2) << "resolved address " << domain() << ": " << endpoint_;
        endpoints_.push_back(*iter);
      }
      DCHECK(!endpoints_.empty());

      on_try_next_endpoint(channel);
    });
  }

  std::string domain() {
    return absl::StrCat(host_name_, ":", std::to_string(port_));
  }

  bool connected() const { return connected_; }

  bool eof() const { return eof_; }

  bool read_inprogress() const {
    return read_inprogress_;
  }

  /// wait read routine
  ///
  void wait_read(handle_t callback, bool yield) {
    DCHECK(!read_inprogress_);
    DCHECK(callback);

    if (UNLIKELY(!connected_ || closed_)) {
      return;
    }

    read_inprogress_ = true;
    wait_read_callback_ = std::move(callback);
    scoped_refptr<stream> self(this);
    if (yield) {
      asio::post(io_context_, [this, self]() {
        handle_t callback = std::move(wait_read_callback_);
        wait_read_callback_ = nullptr;
        read_inprogress_ = false;
        if (UNLIKELY(!connected_ || closed_)) {
          DCHECK(!user_connect_callback_);
          return;
        }
        if (UNLIKELY(!callback)) {
          return;
        }
        callback(asio::error_code());
      });
      return;
    }
    s_wait_read_([this, self](asio::error_code ec) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
        return;
      }
      handle_t callback = std::move(wait_read_callback_);
      wait_read_callback_ = nullptr;
      read_inprogress_ = false;
      if (UNLIKELY(!connected_ || closed_)) {
        DCHECK(!user_connect_callback_);
        return;
      }
      if (UNLIKELY(!callback)) {
        return;
      }
      callback(ec);
    });
  }

  size_t read_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    DCHECK(!closed_ && "I/O on closed upstream connection");
    size_t read = s_read_some_(buf, ec);
    rbytes_transferred_ += read;
    if (UNLIKELY(ec && ec != asio::error::try_again && ec != asio::error::would_block)) {
      on_disconnect(channel_, ec);
    }
    return read;
  }

  bool write_inprogress() const {
    return write_inprogress_;
  }

  /// wait write routine
  ///
  void wait_write(std::function<void(asio::error_code ec)> callback) {
    DCHECK(!write_inprogress_);
    DCHECK(callback);

    write_inprogress_ = true;
    wait_write_callback_ = std::move(callback);
    scoped_refptr<stream> self(this);
    s_wait_write([this, self](asio::error_code ec) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
        return;
      }
      handle_t callback = std::move(wait_write_callback_);
      wait_write_callback_ = nullptr;
      write_inprogress_ = false;
      if (UNLIKELY(!connected_ || closed_)) {
        DCHECK(!user_connect_callback_);
        return;
      }
      if (UNLIKELY(!callback)) {
        return;
      }
      callback(ec);
    });
  }

  size_t write_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    DCHECK(!closed_ && "I/O on closed upstream connection");
    size_t written = s_write_some_(buf, ec);
    wbytes_transferred_ += written;
    if (UNLIKELY(ec && ec != asio::error::try_again && ec != asio::error::would_block)) {
      on_disconnect(channel_, ec);
    }
    return written;
  }

  void close() {
    if (closed_) {
      return;
    }
    closed_ = true;
    connected_ = false;
    eof_ = true;

    user_connect_callback_ = nullptr;
    wait_read_callback_ = nullptr;
    wait_write_callback_ = nullptr;

    asio::error_code ec;
    if (enable_tls_) {
      ssl_socket_->Disconnect();
    } else {
      socket_.close(ec);
    }
    if (ec) {
      VLOG(2) << "close() error: " << ec;
    }
    connect_timer_.cancel();
#ifdef HAVE_C_ARES
    resolver_->Cancel();
#else
    resolver_.cancel();
#endif
  }

  bool https_fallback() const { return https_fallback_; }

 private:
  void on_try_next_endpoint(Channel* channel) {
    DCHECK(!endpoints_.empty());
    asio::ip::tcp::endpoint endpoint = endpoints_.front();
    VLOG(1) << "trying endpoint (" << domain() << "): " << endpoint;
    endpoints_.pop_front();
    endpoint_ = std::move(endpoint);
    if (socket_.is_open()) {
      asio::error_code ec;
      socket_.close(ec);
    }
    on_resolve(channel);
  }

  void on_resolve(Channel* channel) {
    asio::error_code ec;
    socket_.open(endpoint_.protocol(), ec);
    if (ec) {
      if (!endpoints_.empty()) {
        on_try_next_endpoint(channel);
        return;
      }
      closed_ = true;
      on_async_connect_callback(ec);
      return;
    }
    SetTCPFastOpenConnect(socket_.native_handle(), ec);
    SetIPV6Only(socket_.native_handle(), endpoint_.protocol().family(), ec);
    socket_.native_non_blocking(true, ec);
    socket_.non_blocking(true, ec);
    scoped_refptr<stream> self(this);
    if (auto connect_timeout = absl::GetFlag(FLAGS_connect_timeout)) {
      connect_timer_.expires_after(std::chrono::seconds(connect_timeout));
      connect_timer_.async_wait(
        [this, channel, self](asio::error_code ec) {
        // Cancelled, safe to ignore
        if (ec == asio::error::operation_aborted) {
          return;
        }
        on_async_connect_expired(channel, ec);
      });
    }
    socket_.async_connect(endpoint_,
      [this, channel, self](asio::error_code ec) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
        return;
      }
      if (closed_) {
        DCHECK(!user_connect_callback_);
        return;
      }
      if (enable_tls_ && !ec) {
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
          on_async_connected(channel, ec);
          scoped_refptr<stream> self(this);
          // Also queue a ConfirmHandshake. It should also be blocked on ServerHello.
          auto cb = [this, self, channel](int rv){
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
        return;
      }
      on_async_connected(channel, ec);
    });
  }

  void on_async_connected(Channel* channel, asio::error_code ec) {
    connect_timer_.cancel();
    if (ec) {
      if (!endpoints_.empty()) {
        on_try_next_endpoint(channel);
        return;
      }
      on_async_connect_callback(ec);
      return;
    }
    if (enable_tls_) {
      std::string alpn = ssl_socket_->negotiated_protocol();
      if (!alpn.empty()) {
        VLOG(2) << "Alpn selected (client): " << alpn;
      }
      https_fallback_ |= alpn == "http/1.1";
      if (https_fallback_) {
        VLOG(2) << "Alpn fallback to https protocol (client)";
      }
    }
    connected_ = true;
    SetTCPCongestion(socket_.native_handle(), ec);
    SetTCPKeepAlive(socket_.native_handle(), ec);
    SetSocketTcpNoDelay(&socket_, ec);
    on_async_connect_callback(asio::error_code());
  }

  void on_async_connect_expired(Channel* channel,
                                asio::error_code ec) {
    // Rarely happens, cancel fails but expire still there
    if (connected_) {
      DCHECK(!user_connect_callback_);
      return;
    }
    VLOG(1) << "connection timed out with endpoint: " << endpoint_;
    eof_ = true;
    if (!ec) {
      ec = asio::error::timed_out;
    }
    on_async_connect_callback(ec);
  }

  void on_disconnect(Channel* channel,
                     asio::error_code ec) {
    if (ec) {
      VLOG(2) << "data transfer failed with " << endpoint_ << " due to "
              << ec << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
#ifndef NDEBUG
      const char* file;
      int line;
      while (uint32_t error = ERR_get_error_line(&file, &line)) {
        char buf[120];
        ERR_error_string_n(error, buf, sizeof(buf));
        LogMessage(file, line, LOGGING_ERROR).stream()
          << "OpenSSL error: " << buf;
      }
#endif
    } else {
      VLOG(2) << "data transfer closed with: " << endpoint_ << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
    }
    channel->disconnected(ec);
  }

 public:
  size_t rbytes_transferred() const { return rbytes_transferred_; }
  size_t wbytes_transferred() const { return wbytes_transferred_; }

 private:
  /// used to resolve local and remote endpoint
#ifdef HAVE_C_ARES
  scoped_refptr<CAresResolver> resolver_;
#else
  asio::ip::tcp::resolver resolver_;
#endif

  std::string host_name_;
  uint16_t port_;
  asio::ip::tcp::endpoint endpoint_;
  asio::io_context& io_context_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;
  std::deque<asio::ip::tcp::endpoint> endpoints_;

  bool https_fallback_;
  const bool enable_tls_;
  scoped_refptr<net::SSLSocket> ssl_socket_;

  Channel* channel_;
  bool connected_ = false;
  bool eof_ = false;
  bool closed_ = false;
  handle_t user_connect_callback_;

  bool read_inprogress_ = false;
  bool write_inprogress_ = false;
  handle_t wait_read_callback_;
  handle_t wait_write_callback_;

  std::function<void(handle_t)> s_wait_read_;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_read_some_;
  std::function<void(handle_t)> s_wait_write;
  std::function<size_t(std::shared_ptr<IOBuf>, asio::error_code &)> s_write_some_;
  std::function<void(handle_t)> s_async_shutdown_;
  std::function<void(asio::error_code&)> s_shutdown_;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

#endif  // H_STREAM
