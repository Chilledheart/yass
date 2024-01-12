// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_NET_STREAM
#define H_NET_STREAM

#include <chrono>
#include <deque>

#include "channel.hpp"
#include "config/config.hpp"
#include "net/asio.hpp"
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "core/utils.hpp"
#include "net/network.hpp"
#include "net/ssl_socket.hpp"
#include "net/protocol.hpp"

#ifdef HAVE_C_ARES
#include "net/c-ares.hpp"
#endif

#include <absl/functional/any_invocable.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/time/time.h>

namespace net {

/// the class to describe the traffic between given node (endpoint)
class stream : public RefCountedThreadSafe<stream> {
 public:
  using io_handle_t = absl::AnyInvocable<void(asio::error_code, std::size_t)>;
  using handle_t = absl::AnyInvocable<void(asio::error_code)>;

  /// construct a stream object
  template<typename... Args>
  static scoped_refptr<stream> create(Args&&... args) {
    return MakeRefCounted<stream>(std::forward<Args>(args)...);
  }

  /// construct a stream object with ss protocol
  ///
  /// \param io_context the io context associated with the service
  /// \param host_ips the ip addresses used with endpoint
  /// \param host_sni the sni name used with endpoint
  /// \param port the sni port used with endpoint
  /// \param channel the underlying data channel used in stream
  stream(asio::io_context& io_context,
         const std::string& host_ips,
         const std::string& host_sni,
         uint16_t port,
         Channel* channel)
#ifdef HAVE_C_ARES
      : resolver_(CAresResolver::Create(io_context)),
#else
      : resolver_(io_context),
#endif
        host_ips_(host_ips),
        host_sni_(host_sni),
        port_(port),
        io_context_(io_context),
        socket_(io_context),
        connect_timer_(io_context),
        channel_(channel),
        read_yield_timer_(io_context),
        limit_rate_(absl::GetFlag(FLAGS_limit_rate).rate),
        read_delay_timer_(io_context),
        write_delay_timer_(io_context) {
    CHECK(channel && "channel must defined to use with stream");
#ifdef HAVE_C_ARES
    int ret = resolver_->Init(5000);
    CHECK_EQ(ret, 0) << "c-ares initialize failure";
    static_cast<void>(ret);
#endif
  }

  virtual ~stream() {
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

    if (!host_ips_.empty()) {
      auto host_ips = absl::StrSplit(host_ips_, ';');
      for (const auto& host_ip : host_ips) {
        asio::error_code ec;
        auto addr = asio::ip::make_address(host_ip, ec);
        if (ec) {
          LOG(WARNING) << "invalid ip address: " << host_ip;
          continue;
        }
        VLOG(1) << "found ip address (pre-resolved): " << addr;
        endpoints_.emplace_back(addr, port_);
      }
      if (endpoints_.empty()) {
        LOG(WARNING) << "invalid ip addresses: " << host_ips_;
        closed_ = true;
        on_async_connect_callback(asio::error::host_not_found);
      } else {
        on_try_next_endpoint(channel);
      }
      return;
    }

    asio::error_code ec;
    auto addr = asio::ip::make_address(host_sni_.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (host_is_ip_address) {
      VLOG(1) << "resolved ip-like address (post-resolved): " << addr.to_string();
      endpoints_.emplace_back(addr, port_);
      on_try_next_endpoint(channel);
      return;
    }

    scoped_refptr<stream> self(this);
#ifdef HAVE_C_ARES
    resolver_->AsyncResolve(host_sni_, std::to_string(port_),
#else
    resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(),
                            host_sni_, std::to_string(port_),
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
        endpoints_.push_back(*iter);
        VLOG(1) << "found ip address (post-resolved): " << endpoints_.back().address().to_string();
      }
      DCHECK(!endpoints_.empty());

      on_try_next_endpoint(channel);
    });
  }

  std::string domain() {
    return absl::StrCat(host_sni_, ":", std::to_string(port_));
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

    if (limit_rate_) {
      auto delta = absl::ToInt64Seconds(absl::Now() - read_start_);
      int64_t clicks = delta + 1;
      int64_t estimated_transferred;
      if (UNLIKELY(INT64_MAX / (int64_t)limit_rate_ <= clicks)) {
        estimated_transferred = INT64_MAX;
      } else {
        estimated_transferred = limit_rate_ * clicks;
      }
      int64_t limit = estimated_transferred - rbytes_transferred_;
      if (limit <= 0) {
        read_delay_timer_.expires_after(std::chrono::milliseconds(-limit * 1000 / limit_rate_+1));
        read_delay_timer_.async_wait([this, self](asio::error_code ec) {
          if (UNLIKELY(ec == asio::error::operation_aborted)) {
            return;
          }
          auto callback = std::move(wait_read_callback_);
          DCHECK(!wait_read_callback_);
          read_inprogress_ = false;
          // Cancelled, safe to ignore
          wait_read(std::move(callback), false);
        });
        return;
      }
    }

    if (yield) {
      read_yield_timer_.expires_after(std::chrono::microseconds(kReadYieldIntervalUs));
      read_yield_timer_.async_wait([this, self](asio::error_code ec) {
        if (UNLIKELY(ec == asio::error::operation_aborted)) {
          return;
        }
        auto callback = std::move(wait_read_callback_);
        DCHECK(!wait_read_callback_);
        read_inprogress_ = false;
        if (UNLIKELY(!connected_ || closed_)) {
          DCHECK(!user_connect_callback_);
          return;
        }
        // Cancelled, safe to ignore
        wait_read(std::move(callback), false);
      });
      return;
    }
    s_wait_read([this, self](asio::error_code ec) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
        return;
      }
      handle_t callback = std::move(wait_read_callback_);
      DCHECK(!wait_read_callback_);
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
    size_t read = s_read_some(buf, ec);
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
  void wait_write(handle_t callback) {
    DCHECK(!write_inprogress_);
    DCHECK(callback);

    if (UNLIKELY(!connected_ || closed_)) {
      return;
    }

    if (limit_rate_) {
      auto delta = absl::ToInt64Seconds(absl::Now() - write_start_);
      int64_t clicks = delta + 1;
      int64_t estimated_transferred;
      if (UNLIKELY(INT64_MAX / (int64_t)limit_rate_ <= clicks)) {
        estimated_transferred = INT64_MAX;
      } else {
        estimated_transferred = limit_rate_ * clicks;
      }
      int64_t limit = estimated_transferred - wbytes_transferred_;
      if (limit <= 0) {
        scoped_refptr<stream> self(this);
        write_delay_timer_.expires_after(std::chrono::milliseconds(-limit * 1000 / limit_rate_+1));
        wait_write_callback_ = std::move(callback);
        DCHECK(!wait_write_callback_);
        write_delay_timer_.async_wait([this, self](asio::error_code ec) {
          if (UNLIKELY(ec == asio::error::operation_aborted)) {
            return;
          }
          auto callback = std::move(wait_write_callback_);
          wait_write(std::move(callback));
        });
        return;
      }
    }

    write_inprogress_ = true;
    wait_write_callback_ = std::move(callback);
    scoped_refptr<stream> self(this);
    s_wait_write([this, self](asio::error_code ec) {
      // Cancelled, safe to ignore
      if (UNLIKELY(ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted)) {
        return;
      }
      handle_t callback = std::move(wait_write_callback_);
      DCHECK(!wait_write_callback_);
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
    size_t written = s_write_some(buf, ec);
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
    s_close(ec);
    if (ec) {
      VLOG(2) << "close() error: " << ec;
    }
    read_delay_timer_.cancel();
    write_delay_timer_.cancel();
    read_yield_timer_.cancel();
    connect_timer_.cancel();
#ifdef HAVE_C_ARES
    resolver_->Cancel();
#else
    resolver_.cancel();
#endif
  }

  virtual bool https_fallback() const { return false; }

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
    socket_.native_non_blocking(true, ec);
    socket_.non_blocking(true, ec);
    scoped_refptr<stream> self(this);
    if (auto connect_timeout = absl::GetFlag(FLAGS_connect_timeout)) {
      connect_timer_.expires_after(std::chrono::seconds(connect_timeout));
      connect_timer_.async_wait(
        [this, channel, self](asio::error_code ec) {
        // Cancelled, safe to ignore
        if (UNLIKELY(ec == asio::error::operation_aborted)) {
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
      on_async_connected(channel, ec);
    });
  }

 protected:
  virtual void on_async_connected(Channel* channel, asio::error_code ec) {
    connect_timer_.cancel();
    if (ec) {
      if (!endpoints_.empty()) {
        on_try_next_endpoint(channel);
        return;
      }
      on_async_connect_callback(ec);
      return;
    }
    connected_ = true;
    SetTCPCongestion(socket_.native_handle(), ec);
    SetTCPKeepAlive(socket_.native_handle(), ec);
    SetSocketTcpNoDelay(&socket_, ec);
    write_start_ = read_start_ = absl::Now();
    on_async_connect_callback(asio::error_code());
  }

 private:
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
        ::yass::LogMessage(file, line, LOGGING_ERROR).stream()
          << "OpenSSL error: " << buf;
      }
#endif
    } else {
      VLOG(2) << "data transfer closed with: " << endpoint_ << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
    }
    channel->disconnected(ec);
  }

 protected:
  virtual void s_wait_read(handle_t &&cb) {
    socket_.async_wait(asio::ip::tcp::socket::wait_read, std::move(cb));
  }

  virtual size_t s_read_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    return socket_.read_some(tail_buffer(*buf), ec);
  }

  virtual void s_wait_write(handle_t &&cb) {
    socket_.async_wait(asio::ip::tcp::socket::wait_write, std::move(cb));
  }

  virtual size_t s_write_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    return socket_.write_some(const_buffer(*buf), ec);
  }

  virtual void s_async_shutdown(handle_t &&cb) {
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    cb(ec);
  }

  virtual void s_shutdown(asio::error_code &ec) {
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
  }

  virtual void s_close(asio::error_code &ec) {
    socket_.close(ec);
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
 protected:
  std::string host_ips_;
  std::string host_sni_;
  uint16_t port_;
  asio::ip::tcp::endpoint endpoint_;
  asio::io_context& io_context_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;
  std::deque<asio::ip::tcp::endpoint> endpoints_;

  Channel* channel_;
  bool connected_ = false;
  bool eof_ = false;
  bool closed_ = false;
  handle_t user_connect_callback_;

 private:
  bool read_inprogress_ = false;
  bool write_inprogress_ = false;
  handle_t wait_read_callback_;
  handle_t wait_write_callback_;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;

  // post yield
  asio::steady_timer read_yield_timer_;
#if BUILDFLAG(IS_IOS)
  // Every full mtu 1500 bytes packet arrives in every 100us
  // the maximum traffer rate is 1500 b / 100 us = 14.3 MB/s
  static constexpr uint64_t kReadYieldIntervalUs = 100;
#else
  static constexpr uint64_t kReadYieldIntervalUs = 10;
#endif

  // rate limiter
  const uint64_t limit_rate_;
  absl::Time read_start_;
  asio::steady_timer read_delay_timer_;

  absl::Time write_start_;
  asio::steady_timer write_delay_timer_;
};

} // namespace net

#endif  // H_STREAM
