//
// socks5_connection.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks5_connection.hpp"

#include "cipher.hpp"
#include "config.hpp"

extern cipher::cipher_method cipher_method;

namespace socks5 {

Socks5Connection::Socks5Connection(
    boost::asio::io_context &io_context, boost::asio::ip::tcp::socket &&socket,
    const boost::asio::ip::tcp::endpoint &endpoint,
    const boost::asio::ip::tcp::endpoint &peer_endpoint,
    const boost::asio::ip::tcp::endpoint &remote_endpoint)
    : io_context_(io_context), socket_(std::move(socket)), state_(),
      endpoint_(endpoint), peer_endpoint_(peer_endpoint),
      remote_endpoint_(remote_endpoint),
      encoder_(new cipher("", FLAGS_password, cipher_method, true)),
      decoder_(new cipher("", FLAGS_password, cipher_method)) {
  upstream_writable_ = false;
  downstream_writable_ = false;
}

Socks5Connection::~Socks5Connection() { close(); }

void Socks5Connection::start() {
  channel_ = std::make_unique<ss::stream>(
      io_context_, remote_endpoint_,
      std::static_pointer_cast<Channel>(shared_from_this()));
  setState(state_method_select);
  upstream_writable_ = true;
  downstream_writable_ = true;
  start_read();
}

boost::system::error_code Socks5Connection::onCmdConnect(ByteRange vaddress,
                                                         reply *reply) {
  reply->mutable_status() = reply::request_granted;

  channel_->connect();
  // write variable address directly as ss header
  std::unique_ptr<IOBuf> buf = IOBuf::copyBuffer(vaddress);
  std::shared_ptr<IOBuf> sharedBuf{buf.release()};
  performUpstreamWrite(sharedBuf);

  return boost::system::errc::make_error_code(boost::system::errc::success);
}

void Socks5Connection::onConnect() {
  VLOG(2) << "socks5: established connection with: " << endpoint_;
}

void Socks5Connection::onStreamRead(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "socks5: read: " << buf->length() << " bytes.";
  performUpstreamWrite(buf);
}

void Socks5Connection::onStreamWrite(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "socks5: sent reply: " << buf->length() << " bytes.";
  downstream_writable_ = true;

  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();

  /* recursively send the remainings */
  performDownstreamFlush();
}

void Socks5Connection::onDisconnect(boost::system::error_code error) {
  VLOG(2) << "socks5: lost connection with: " << endpoint_;
  close();
}

void Socks5Connection::performDownstreamFlush() {
  std::shared_ptr<IOBuf> buf{IOBuf::create(0).release()};
  performDownstreamWrite(buf);
}

void Socks5Connection::performDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (!buf->empty()) {
    downstream_.push_back(buf);
  }
  if (!downstream_.empty() && downstream_writable_) {
    std::shared_ptr<IOBuf> buf = downstream_[0];
    downstream_writable_ = false;
    start_write(buf);
  }
}

void Socks5Connection::performUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (!buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    std::shared_ptr<IOBuf> buf = upstream_[0];
    upstream_writable_ = false;

    std::unique_ptr<IOBuf> cipherbuf = IOBuf::create(buf->length());
#ifndef NDEBUG
    DumpHex("PWrite->", buf.get());
#endif
    encoder_->encrypt(buf.get(), cipherbuf);
#ifndef NDEBUG
    DumpHex("EWrite->", cipherbuf.get());
#endif
    std::shared_ptr<IOBuf> sharedBuf{cipherbuf.release()};
    channel_->start_write(sharedBuf);
  }
}

void Socks5Connection::performUpstreamFlush() {
  std::shared_ptr<IOBuf> buf{IOBuf::create(0).release()};
  performUpstreamWrite(buf);
}

void Socks5Connection::start_read() {
  switch (state_) {
  case state_method_select:
    readMethodSelect();
    break;
  case state_handshake:
    readHandshake();
    break;
  case state_stream:
    readStream();
    break;
  case state_error:
    break;
  }
}

void Socks5Connection::start_write(std::shared_ptr<IOBuf> buf) {
  writeStream(buf);
}

void Socks5Connection::readMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  socket_.async_read_some(
      boost::asio::buffer(buffer_),
      [this, self](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          onDisconnect(error);
          return;
        }
        method_select_request_parser::result_type result;
        std::tie(result, std::ignore) = method_select_request_parser_.parse(
            method_select_request_, buffer_.data(),
            buffer_.data() + bytes_transferred);

        if (result == method_select_request_parser::good) {
          processReceivedData(self, nullptr, error, bytes_transferred);
        } else if (result == method_select_request_parser::bad) {
          onDisconnect(error);
        } else {
          readMethodSelect();
        }
      });
}

void Socks5Connection::readHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  socket_.async_read_some(
      boost::asio::buffer(buffer_),
      [this, self](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          onDisconnect(error);
          return;
        }
        request_parser::result_type result;
        std::tie(result, std::ignore) = request_parser_.parse(
            request_, buffer_.data(), buffer_.data() + bytes_transferred);

        if (result == request_parser::good) {
          processReceivedData(self, nullptr, error, bytes_transferred);
        } else if (result == request_parser::bad) {
          onDisconnect(error);
        } else {
          readHandshake();
        }
      });
}

void Socks5Connection::readStream() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [this, buf, self](const boost::system::error_code &error,
                        std::size_t bytes_transferred) -> std::size_t {
        if (!error) {
          VLOG(4) << "remaining available " << socket_.available()
                  << " bytes transferred: " << bytes_transferred << " bytes.";
        }
        if (bytes_transferred || error) {
          processReceivedData(self, buf, error, bytes_transferred);
          return 0;
        }
        return SOCKET_BUF_SIZE;
      });
}

void Socks5Connection::writeMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  method_select_reply_ = method_select_response::stock_reply();
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
      std::bind(&Socks5Connection::processSentData, this, self, nullptr,
                std::placeholders::_1, std::placeholders::_2));
}

void Socks5Connection::writeHandshake(reply *reply) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  boost::asio::async_write(socket_, reply->buffers(),
                           std::bind(&Socks5Connection::processSentData, this,
                                     self, nullptr, std::placeholders::_1,
                                     std::placeholders::_2));
}

void Socks5Connection::writeStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  boost::asio::async_write(
      socket_, boost::asio::buffer(buf->data(), buf->length()),
      std::bind(&Socks5Connection::processSentData, this, self, buf,
                std::placeholders::_1, std::placeholders::_2));
}

boost::system::error_code
Socks5Connection::performCmdOps(const std::shared_ptr<Socks5Connection> &self,
                                command_type command, reply *reply) {
  if (request_.address_type() == domain) {
    ss_request_ =
        std::make_unique<ss::request>(request_.domain_name(), request_.port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request_.endpoint());
  }

  ByteRange vaddress(ss_request_->data(), ss_request_->length());
  switch (command) {
  case cmd_connect:
    return self->onCmdConnect(vaddress, reply);
  case cmd_bind:
  case cmd_udp_associate:
  default:
    break;
  }
  return boost::system::errc::make_error_code(boost::system::errc::bad_message);
}

void Socks5Connection::processReceivedData(
    const std::shared_ptr<Socks5Connection> &self, std::shared_ptr<IOBuf> buf,
    boost::system::error_code error, size_t bytes_transferred) {
  rbytes_transferred_ += bytes_transferred;
  if (bytes_transferred) {
    VLOG(4) << "received request: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (state_) {
    case state_method_select:
      writeMethodSelect();
      break;
    case state_handshake:
      error = performCmdOps(self, static_cast<command_type>(request_.command()),
                            &reply_);

      if (error) {
        reply_.mutable_status() = reply::request_failed_cmd_not_supported;
        reply_.set_endpoint(request_.endpoint());
      } else {
        boost::asio::ip::tcp::endpoint endpoint = request_.endpoint();
        reply_.mutable_status() = reply::request_granted;
        if (request_.address_type() == domain) {
          // TBD async resolve/prevent DNS leak
          // TBD fix AAAA record
          // Get a list of endpoints corresponding to the SOCKS 5 domain name.
          boost::asio::ip::tcp::resolver resolver(io_context_);
          auto endpoints = resolver.resolve(request_.domain_name(),
                                            std::to_string(request_.port()),
                                            error);
          endpoint = endpoints->endpoint();
          if (error) {
            LOG(ERROR) << "[dns] reply with error: " << error
              << " for domain " << request_.domain_name();
          } else {
            LOG(WARNING) << "[dns] reply with endpoint: " << endpoint
              << " for domain " << request_.domain_name();
          }
        }
        reply_.set_endpoint(endpoint);
      }
      writeHandshake(&reply_);
      break;
    case state_stream:
      buf->append(bytes_transferred);
      if (bytes_transferred) {
        onStreamRead(buf);
      }
      start_read(); // continously read
      break;
    case state_error:
      error = boost::system::errc::make_error_code(
          boost::system::errc::bad_message);
      break;
    };
  }
  if (error) {
    VLOG(2) << "broken connection due to " << error;
    setState(state_error);
    onDisconnect(error);
  }
};

void Socks5Connection::processSentData(
    const std::shared_ptr<Socks5Connection> &self, std::shared_ptr<IOBuf> buf,
    boost::system::error_code error, size_t bytes_transferred) {
  wbytes_transferred_ += bytes_transferred;

  if (bytes_transferred) {
    VLOG(4) << "process sent data: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (state_) {
    case state_method_select:
      setState(state_handshake);
      start_read(); // read next state info
      break;
    case state_handshake:
      setState(state_stream);
      onConnect();
      start_read(); // read next state info
      break;
    case state_stream:
      onStreamWrite(buf);
      break;
    case state_error:
      error = boost::system::errc::make_error_code(
          boost::system::errc::bad_message);
      break;
    }
  }

  if (error) {
    VLOG(2) << "data transfer failed due to " << error;
    setState(state_error);
    onDisconnect(error);
  }

  // TBD a good idea is to put it into a memory pool
};

void Socks5Connection::connected() {
  VLOG(2) << "remote: established connection with: " << remote_endpoint_;
  channel_->start_read();
  performDownstreamFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: received reply: " << buf->length() << " bytes.";
  std::unique_ptr<IOBuf> plainbuf = IOBuf::create(buf->length());

#ifndef NDEBUG
  DumpHex("ERead->", buf.get());
#endif
  decoder_->decrypt(buf.get(), plainbuf);
#ifndef NDEBUG
  DumpHex("PRead->", plainbuf.get());
#endif

  std::shared_ptr<IOBuf> sharedbuf{plainbuf.release()};

  performDownstreamWrite(sharedbuf);
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: sent reply: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty());
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  performUpstreamFlush();
}

void Socks5Connection::disconnected(boost::system::error_code error) {
  VLOG(2) << "upstream: lost connection with: " << remote_endpoint_;
  close();
}

} // namespace socks5
