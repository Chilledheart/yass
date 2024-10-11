// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_NET_STREAM
#define H_NET_STREAM

#include <chrono>
#include <deque>

#include "channel.hpp"
#include "config/config_network.hpp"
#include "config/config_ptype.hpp"
#include "core/logging.hpp"
#include "core/scoped_refptr.hpp"
#include "core/utils.hpp"
#include "net/asio.hpp"
#include "net/network.hpp"
#include "net/protocol.hpp"
#include "net/resolver.hpp"
#include "net/ssl_socket.hpp"

#ifdef __OHOS__
#include "harmony/yass.hpp"
#endif

#include <absl/functional/any_invocable.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>
#include <absl/time/time.h>

namespace net {

/* check rate limits within this many recent milliseconds, at minimum. */
#define MIN_RATE_LIMIT_PERIOD 3000

// modified from curl's lib/multi.c
inline int64_t pgrsLimitWaitTime(int64_t cursize, int64_t startsize, int64_t limit, absl::Time start, absl::Time now) {
  int64_t size = cursize - startsize;
  int64_t minimum;
  int64_t actual;

  if (!limit || !size)
    return 0;

  /*
   * 'minimum' is the number of milliseconds 'size' should take to download to
   * stay below 'limit'.
   */
  if (size < INT64_MAX / 1000)
    minimum = (int64_t)(int64_t(1000) * size / limit);
  else {
    minimum = (int64_t)(size / limit);
    if (minimum < INT64_MAX / 1000)
      minimum *= 1000;
    else
      minimum = INT64_MAX;
  }

  /*
   * 'actual' is the time in milliseconds it took to actually download the
   * last 'size' bytes.
   */
  actual = absl::ToInt64Milliseconds(now - start);
  if (actual < minimum) {
    /* if it downloaded the data faster than the limit, make it wait the
       difference */
    return (minimum - actual);
  }

  return 0;
}

/// the class to describe the traffic between given node (endpoint)
class stream : public RefCountedThreadSafe<stream> {
 public:
  using io_handle_t = absl::AnyInvocable<void(asio::error_code, std::size_t)>;
  using handle_t = absl::AnyInvocable<void(asio::error_code)>;

  /// construct a stream object
  template <typename... Args>
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
      : resolver_(io_context),
        host_ips_(host_ips),
        host_sni_(host_sni),
        port_(port),
        io_context_(io_context),
        socket_(io_context),
        connect_timer_(io_context),
        channel_(channel),
        read_yield_timer_(io_context),
        dl_limit_rate_(absl::GetFlag(FLAGS_limit_rate).rate),
        dl_delay_timer(io_context),
        ul_limit_rate_(absl::GetFlag(FLAGS_limit_rate).rate),
        ul_delay_timer(io_context) {
    CHECK(channel && "channel must defined to use with stream");
  }

  virtual ~stream() { close(); }

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

    if (port_ == 0u) {
      closed_ = true;
      on_async_connect_callback(asio::error::network_unreachable);
      return;
    }

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

    int ret = resolver_.Init();
    if (ret < 0) {
      LOG(WARNING) << "resolver initialize failure";
      closed_ = true;
      on_async_connect_callback(asio::error::host_not_found);
      return;
    }

    scoped_refptr<stream> self(this);
    resolver_.AsyncResolve(
        host_sni_, port_,
        [this, channel, self](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
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

  std::string domain() { return absl::StrCat(host_sni_, ":", port_); }

  bool connected() const { return connected_; }

  bool eof() const { return eof_; }

  bool read_inprogress() const { return read_inprogress_; }

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

    if (dl_limit_rate_) {
      auto recv_timeout_ms =
          pgrsLimitWaitTime(rbytes_transferred_, dl_limit_size_, dl_limit_rate_, dl_limit_start_, absl::Now());
      if (recv_timeout_ms) {
        if (!ul_limit_state_ && !dl_limit_state_) {
          // entering ratelimit state
          ratelimit(absl::Now());
        }
        dl_limit_state_ = true;
        dl_delay_timer.expires_after(std::chrono::milliseconds(recv_timeout_ms));
        dl_delay_timer.async_wait([this, self](asio::error_code ec) {
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

    if (dl_limit_state_ && !ul_limit_state_) {
      // leaving ratelimit state
      ratelimit(absl::Now());
    }
    dl_limit_state_ = false;

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

  size_t read_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
    DCHECK(!closed_ && "I/O on closed upstream connection");
    size_t read = s_read_some(buf, ec);
    rbytes_transferred_ += read;
    if (UNLIKELY(ec && ec != asio::error::try_again && ec != asio::error::would_block)) {
      on_disconnect(channel_, ec);
    }
    return read;
  }

  bool write_inprogress() const { return write_inprogress_; }

  /// wait write routine
  ///
  void wait_write(handle_t callback) {
    DCHECK(!write_inprogress_);
    DCHECK(callback);

    if (UNLIKELY(!connected_ || closed_)) {
      return;
    }

    if (ul_limit_rate_) {
      auto send_timeout_ms =
          pgrsLimitWaitTime(wbytes_transferred_, ul_limit_size_, ul_limit_rate_, ul_limit_start_, absl::Now());
      if (send_timeout_ms) {
        if (!ul_limit_state_ && !dl_limit_state_) {
          // entering ratelimit state
          ratelimit(absl::Now());
        }
        ul_limit_state_ = true;
        scoped_refptr<stream> self(this);
        ul_delay_timer.expires_after(std::chrono::milliseconds(send_timeout_ms));
        wait_write_callback_ = std::move(callback);
        DCHECK(!wait_write_callback_);
        ul_delay_timer.async_wait([this, self](asio::error_code ec) {
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
    if (ul_limit_state_ && !dl_limit_state_) {
      // leaving ratelimit state
      ratelimit(absl::Now());
    }
    ul_limit_state_ = false;
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

  size_t write_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
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
    dl_delay_timer.cancel();
    ul_delay_timer.cancel();
    read_yield_timer_.cancel();
    connect_timer_.cancel();
    resolver_.Cancel();
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
    if (endpoint_.address().is_unspecified() || endpoint_.address().is_multicast()) {
      if (!endpoints_.empty()) {
        on_try_next_endpoint(channel);
        return;
      }
      closed_ = true;
      on_async_connect_callback(asio::error::network_unreachable);
      return;
    }
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
#ifdef __OHOS__
    setProtectFd(socket_.native_handle());
#endif
    SetTCPFastOpenConnect(socket_.native_handle(), ec);
    socket_.non_blocking(true, ec);
    scoped_refptr<stream> self(this);
    if (auto connect_timeout = absl::GetFlag(FLAGS_connect_timeout)) {
      connect_timer_.expires_after(std::chrono::seconds(connect_timeout));
      connect_timer_.async_wait([this, channel, self](asio::error_code ec) {
        // Cancelled, safe to ignore
        if (UNLIKELY(ec == asio::error::operation_aborted)) {
          return;
        }
        on_async_connect_expired(channel, ec);
      });
    }
    socket_.async_connect(endpoint_, [this, channel, self](asio::error_code ec) {
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
    if (config::pType_IsClient()) {
      SetTCPCongestion(socket_.native_handle(), ec);
      SetTCPKeepAlive(socket_.native_handle(), ec);
    }
    SetSocketTcpNoDelay(&socket_, ec);

    auto start = absl::Now();
    ul_limit_size_ = dl_limit_size_ = 0;
    ul_limit_start_ = dl_limit_start_ = start;
    ul_limit_state_ = dl_limit_state_ = false;
    ratelimit(start);
    on_async_connect_callback(asio::error_code());
  }

 private:
  void on_async_connect_expired(Channel* channel, asio::error_code ec) {
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

  void on_disconnect(Channel* channel, asio::error_code ec) {
    if (ec) {
      VLOG(2) << "data transfer failed with " << endpoint_ << " due to " << ec << " stats: readed "
              << rbytes_transferred_ << " written: " << wbytes_transferred_;
#ifndef NDEBUG
      const char* file;
      int line;
      while (uint32_t error = ERR_get_error_line(&file, &line)) {
        char buf[120];
        ERR_error_string_n(error, buf, sizeof(buf));
        ::yass::LogMessage(file, line, LOGGING_ERROR).stream() << "OpenSSL error: " << buf;
      }
#endif
    } else {
      VLOG(2) << "data transfer closed with: " << endpoint_ << " stats: readed " << rbytes_transferred_
              << " written: " << wbytes_transferred_;
    }
    channel->disconnected(ec);
  }

 protected:
  virtual void s_wait_read(handle_t&& cb) { socket_.async_wait(asio::ip::tcp::socket::wait_read, std::move(cb)); }

  virtual size_t s_read_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
    return socket_.read_some(tail_buffer(*buf), ec);
  }

  virtual void s_wait_write(handle_t&& cb) { socket_.async_wait(asio::ip::tcp::socket::wait_write, std::move(cb)); }

  virtual size_t s_write_some(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
    return socket_.write_some(const_buffer(*buf), ec);
  }

  virtual void s_async_shutdown(handle_t&& cb) {
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    cb(ec);
  }

  virtual void s_shutdown(asio::error_code& ec) { socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec); }

  virtual void s_close(asio::error_code& ec) { socket_.close(ec); }

 public:
  size_t rbytes_transferred() const { return rbytes_transferred_; }
  size_t wbytes_transferred() const { return wbytes_transferred_; }

 private:
  /*
   * Update the timestamp and sizestamp to use for rate limit calculations.
   */
  void ratelimit(absl::Time now) {
    /* do not set a new stamp unless the time since last update is long enough */
    if (dl_limit_rate_) {
      if (absl::ToInt64Milliseconds(now - dl_limit_start_) >= MIN_RATE_LIMIT_PERIOD) {
        dl_limit_start_ = now;
        dl_limit_size_ = rbytes_transferred_;
      }
    }
    if (ul_limit_rate_) {
      if (absl::ToInt64Milliseconds(now - ul_limit_start_) >= MIN_RATE_LIMIT_PERIOD) {
        ul_limit_start_ = now;
        ul_limit_size_ = wbytes_transferred_;
      }
    }
  }

 private:
  /// used to resolve local and remote endpoint
  net::Resolver resolver_;

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
  int64_t rbytes_transferred_ = 0;
  int64_t wbytes_transferred_ = 0;

  // post yield
  asio::steady_timer read_yield_timer_;
  static constexpr const uint64_t kReadYieldIntervalUs = 10;

  // rate limiter (download)
  const int64_t dl_limit_rate_;
  asio::steady_timer dl_delay_timer;
  absl::Time dl_limit_start_;
  int64_t dl_limit_size_;
  bool dl_limit_state_;

  // rate limiter (upload)
  const int64_t ul_limit_rate_;
  asio::steady_timer ul_delay_timer;
  absl::Time ul_limit_start_;
  int64_t ul_limit_size_;
  bool ul_limit_state_;
};

}  // namespace net

#endif  // H_STREAM
