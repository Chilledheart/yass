// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_STREAM
#define H_STREAM

#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>
#include <deque>
#include <chrono>

#include "channel.hpp"
#include "core/logging.hpp"
#include "config/config.hpp"
#include "network.hpp"

/// the class to describe the traffic between given node (endpoint)
class stream {
public:
  /// construct a stream object with ss protocol
  /// \param io_context
  /// \param endpoint
  /// \param channel
  stream(asio::io_context &io_context, asio::ip::tcp::endpoint endpoint,
         const std::shared_ptr<Channel> &channel)
      : endpoint_(endpoint), socket_(io_context), connect_timer_(io_context),
      channel_(channel) {
    assert(channel && "channel must defined to use with stream");
  }

  ~stream() { close(); }

  void connect() {
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    connected_ = false;
    eof_ = false;
    read_enabled_ = true;
    SetTCPFastOpenConnect(socket_.native_handle());
    connect_timer_.expires_from_now(std::chrono::milliseconds(FLAGS_timeout));
    connect_timer_.async_wait(std::bind(&stream::on_connect_expired, this,
                                        std::placeholders::_1));
    socket_.async_connect(endpoint_, std::bind(&stream::on_connect, this,
                                               channel, std::placeholders::_1));
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
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
    buf->reserve(0, SOCKET_BUF_SIZE);
    read_inprogress_ = true;

    socket_.async_read_some(
        asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
        [this, buf, channel](const asio::error_code &error,
                             std::size_t bytes_transferred) -> std::size_t {
          if (bytes_transferred || error) {
            read_inprogress_ = false;
            on_read(channel, buf, error, bytes_transferred);
            return 0;
          }
          return SOCKET_BUF_SIZE;
        });
  }

  void start_write(std::shared_ptr<IOBuf> buf) {
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    asio::async_write(socket_, asio::const_buffer(buf->data(), buf->length()),
                      std::bind(&stream::on_write, this, channel, buf,
                                std::placeholders::_1, std::placeholders::_2));
  }

  void close() {
    asio::error_code ec;
    socket_.close(ec);
    if (ec) {
      LOG(WARNING) << "close() error: " << ec;
    }
  }

private:
  void on_connect(const std::shared_ptr<Channel> &channel,
                  asio::error_code error) {
    connect_timer_.cancel(error);
    if (error) {
      channel->disconnected(error);
      return;
    }
    SetTCPCongestion(socket_.native_handle());
    SetTCPUserTimeout(socket_.native_handle());
    SetSocketLinger(&socket_);
    connected_ = true;
    channel->connected();
  }

  void on_connect_expired(asio::error_code error) {
    if (connected_) {
      return;
    }
    if (error) {
      close();
      return;
    }
    LOG(WARNING) << "connection timed out with endpoint: " << endpoint_;
  }

  void on_read(const std::shared_ptr<Channel> &channel,
               std::shared_ptr<IOBuf> buf, asio::error_code error,
               size_t bytes_transferred) {
    rbytes_transferred_ += bytes_transferred;
    buf->append(bytes_transferred);

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
        VLOG(1) << "data receiving failed with data " << endpoint_ << " due to " << error;
      }
      if (error == asio::error::eof) {
        eof_ = true;
      }
      on_disconnect(channel, error);
    }
  }

  void on_write(const std::shared_ptr<Channel> &channel,
                std::shared_ptr<IOBuf> buf, asio::error_code error,
                size_t bytes_transferred) {
    wbytes_transferred_ += bytes_transferred;

    if (!connected_) {
      return;
    }

    if (bytes_transferred) {
      DCHECK_EQ(bytes_transferred, buf->length());
      channel->sent(buf);
    }

    if (error) {
      if (bytes_transferred) {
        VLOG(1) << "data sending failed with data " << endpoint_ << " due to " << error;
      }
      if (error == asio::error::eof) {
        eof_ = true;
      }
      on_disconnect(channel, error);
    }
  }

  void on_disconnect(const std::shared_ptr<Channel> &channel,
                     asio::error_code error) {
    VLOG(2) << "data transfer failed with " << endpoint_ << " due to " << error;
    connected_ = false;
    channel->disconnected(error);
  }

private:
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::socket socket_;
  asio::steady_timer connect_timer_;

  std::weak_ptr<Channel> channel_;
  bool connected_;
  bool eof_;

  bool read_enabled_;
  bool read_inprogress_ = false;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

#endif // H_STREAM
