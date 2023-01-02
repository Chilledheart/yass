// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

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
 public:
  /// construct a stream object with ss protocol
  ///
  /// \param io_context the io context associated with the service
  /// \param endpoint the endpoint of the service socket
  /// \param channel the underlying data channel used in stream
  stream(asio::io_context& io_context,
         asio::ip::tcp::endpoint endpoint,
         Channel* channel)
      : endpoint_(endpoint),
        socket_(io_context),
        connect_timer_(io_context),
        channel_(channel) {
    assert(channel && "channel must defined to use with stream");
  }

  ~stream() {
    if (!eof_) {
      close();
    }
  }

  void connect() {
    Channel* channel = channel_;
    asio::error_code ec;
    SetTCPFastOpenConnect(socket_.native_handle(), ec);
    connect_timer_.expires_from_now(
        std::chrono::milliseconds(absl::GetFlag(FLAGS_connect_timeout)));
    connect_timer_.async_wait([this, channel](asio::error_code ec) {
      on_connect_expired(channel, ec);
    });
    socket_.async_connect(endpoint_, [this, channel](asio::error_code ec) {
      on_connect(channel, ec);
    });
    socket_.non_blocking(true, ec);
  }

  bool connected() const { return connected_; }

  bool eof() const { return eof_; }

  void disable_read() { read_enabled_ = false; }

  void enable_read() {
    if (!read_enabled_) {
      read_enabled_ = true;
      if (!read_inprogress_) {
        start_read();
      }
    }
  }

  void start_read() {
    Channel* channel = channel_;
    std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
    buf->reserve(0, SOCKET_BUF_SIZE);
    read_inprogress_ = true;

    socket_.async_read_some(mutable_buffer(*buf),
        [this, buf, channel](asio::error_code error,
                             std::size_t bytes_transferred) -> std::size_t {
          if (bytes_transferred || error) {
            read_inprogress_ = false;
            on_read(channel, buf, error, bytes_transferred);
            return 0;
          }
          return SOCKET_BUF_SIZE;
        });
  }

  size_t read_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    return socket_.read_some(mutable_buffer(*buf), ec);
  }

  /// start write routine
  ///
  /// \param buf the shared buffer used in write routine
  void start_write(std::shared_ptr<IOBuf> buf) {
    Channel* channel = channel_;
    asio::async_write(socket_, const_buffer(*buf),
        [this, channel, buf](asio::error_code error, size_t bytes_transferred) {
          on_write(channel, buf, error, bytes_transferred);
        });
  }

  size_t write_some(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
    return socket_.write_some(const_buffer(*buf), ec);
  }

  void close() {
    if (closed_) {
      return;
    }
    eof_ = true;
    closed_ = true;
    asio::error_code ec;
    socket_.cancel(ec);
    if (ec) {
      VLOG(2) << "cancel() error: " << ec;
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
               asio::error_code error,
               size_t bytes_transferred) {
    rbytes_transferred_ += bytes_transferred;
    buf->append(bytes_transferred);

    if (error || bytes_transferred == 0) {
      eof_ = true;
    }

    if (!connected_) {
      return;
    }

    if (bytes_transferred) {
      channel->received(buf);
      if (read_enabled_) {
        start_read();
      }
    }

    if (error) {
      if (bytes_transferred) {
        VLOG(1) << "data receiving failed with data " << endpoint_ << " due to "
                << error;
      }
      on_disconnect(channel, error);
    }
  }

  void on_write(Channel* channel,
                std::shared_ptr<IOBuf> buf,
                asio::error_code error,
                size_t bytes_transferred) {
    wbytes_transferred_ += bytes_transferred;

    if (error || bytes_transferred == 0) {
      eof_ = true;
    }

    if (!connected_) {
      return;
    }

    if (bytes_transferred) {
      DCHECK_LE(bytes_transferred, buf->length());
      channel->sent(buf);
    }

    if (error) {
      if (bytes_transferred) {
        VLOG(1) << "data sending failed with data " << endpoint_ << " due to "
                << error;
      }
      on_disconnect(channel, error);
    }
  }

  void on_disconnect(Channel* channel,
                     asio::error_code error) {
    if (error) {
      VLOG(2) << "data transfer failed with " << endpoint_ << " due to "
              << error;
    }
    VLOG(2) << "data transfer closed with: " << endpoint_ << " stats: readed "
            << rbytes_transferred_ << " written: " << wbytes_transferred_;
    connected_ = false;
    channel->disconnected(error);
  }

 private:
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;

  Channel* channel_;
  bool connected_ = false;
  bool eof_ = false;
  bool closed_ = false;

  bool read_enabled_ = true;
  bool read_inprogress_ = false;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

#endif  // H_STREAM
