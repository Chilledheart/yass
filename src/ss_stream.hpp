//
// ss.hpp
// ~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SS_STREAM
#define H_SS_STREAM

#include "ss.hpp"

#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <deque>
#include <glog/logging.h>

namespace ss {

/// the class to describe the traffic between given node (endpoint)
class stream {
public:
  /// construct a stream object with ss protocol
  /// \param io_context
  /// \param endpoint
  /// \param channel
  stream(boost::asio::io_context &io_context,
         boost::asio::ip::tcp::endpoint endpoint,
         const std::shared_ptr<Channel> &channel)
      : socket_(io_context), endpoint_(endpoint), channel_(channel) {
    assert(channel && "channel must defined to use with stream");
  }

  ~stream() { close(); }

  void connect() {
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    connected_ = false;
    socket_.async_connect(endpoint_, std::bind(&stream::on_connect, this,
                                               channel, std::placeholders::_1));
  }

  void start_read() {
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
    buf->reserve(0, SOCKET_BUF_SIZE);

    socket_.async_read_some(
        boost::asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
        [this, buf, channel](const boost::system::error_code &error,
                             std::size_t bytes_transferred) -> std::size_t {
          if (!error) {
            // LOG(WARNING) << "socket available " << socket_.available() << "
            // vs transferred "
            //             << bytes_transferred;
          }
          if (bytes_transferred || error) {
            on_read(channel, buf, error, bytes_transferred);
            return 0;
          }
          return SOCKET_BUF_SIZE;
        });
  }

  void start_write(std::shared_ptr<IOBuf> buf) {
    std::shared_ptr<Channel> channel = std::shared_ptr<Channel>(channel_);
    boost::asio::async_write(
        socket_, boost::asio::const_buffer(buf->data(), buf->length()),
        std::bind(&stream::on_write, this, channel, buf, std::placeholders::_1,
                  std::placeholders::_2));
  }

  void cancel() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    if (ec) {
      LOG(WARNING) << "cancel() error: " << ec;
    }
  }

  void close() {
    boost::system::error_code ec;
    socket_.close(ec);
    if (ec) {
      LOG(WARNING) << "close() error: " << ec;
    }
  }

private:
  void on_connect(const std::shared_ptr<Channel> &channel,
                  boost::system::error_code error) {
    if (error) {
      channel->disconnected(error);
      return;
    }
    connected_ = true;
    channel->connected();
  }

  void on_read(const std::shared_ptr<Channel> &channel,
               std::shared_ptr<IOBuf> buf, boost::system::error_code error,
               size_t bytes_transferred) {
    rbytes_transferred_ += bytes_transferred;
    buf->append(bytes_transferred);

    if (!connected_) {
      return;
    }

    if (!error) {
      channel->received(buf);
      start_read();
    }

    if (error) {
      on_disconnect(channel, error);
    }
  }

  void on_write(const std::shared_ptr<Channel> &channel,
                std::shared_ptr<IOBuf> buf, boost::system::error_code error,
                size_t bytes_transferred) {
    wbytes_transferred_ += bytes_transferred;

    if (!connected_) {
      return;
    }

    if (!error) {
      DCHECK_EQ(bytes_transferred, buf->length());
      channel->sent(buf);
    }

    if (error) {
      on_disconnect(channel, error);
    }
  }

  void on_disconnect(const std::shared_ptr<Channel> &channel,
                     boost::system::error_code error) {
    VLOG(2) << "data transfer failed with " << endpoint_ << " due to " << error;
    connected_ = false;
    channel->disconnected(error);
  }

private:
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::socket socket_;

  std::weak_ptr<Channel> channel_;
  bool connected_;

  // statistics
  size_t rbytes_transferred_ = 0;
  size_t wbytes_transferred_ = 0;
};

} // namespace ss

#endif // H_SS_STREAM
