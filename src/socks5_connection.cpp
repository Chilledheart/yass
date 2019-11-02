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
    boost::asio::io_context &io_context,
    const boost::asio::ip::tcp::endpoint &remote_endpoint)
    : Connection(io_context, remote_endpoint), state_(),
      encoder_(new cipher("", FLAGS_password, cipher_method, true)),
      decoder_(new cipher("", FLAGS_password, cipher_method)) {}

Socks5Connection::~Socks5Connection() {}

void Socks5Connection::start() {
  channel_ = std::make_unique<ss::stream>(
      io_context_, remote_endpoint_,
      std::static_pointer_cast<Channel>(shared_from_this()));
  SetState(state_method_select);
  closed_ = false;
  upstream_writable_ = true;
  downstream_writable_ = true;
  ReadMethodSelect();
}

void Socks5Connection::close() {
  if (closed_) {
    return;
  }
  LOG(WARNING) << "disconnected with client at stage: "
               << Socks5Connection::state_to_str(CurrentState());
  boost::system::error_code ec;
  closed_ = true;
  socket_.close(ec);
  channel_->close();
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void Socks5Connection::ReadMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  socket_.async_read_some(
      boost::asio::buffer(buffer_),
      [self](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        method_select_request_parser::result_type result;
        std::tie(result, std::ignore) =
            self->method_select_request_parser_.parse(
                self->method_select_request_, self->buffer_.data(),
                self->buffer_.data() + bytes_transferred);

        if (result == method_select_request_parser::good) {
          DCHECK_EQ(self->method_select_request_.length(), bytes_transferred);
          self->ProcessReceivedData(self, nullptr, error, bytes_transferred);
        } else if (result == method_select_request_parser::bad) {
          self->OnDisconnect(error);
        }
      });
}

void Socks5Connection::ReadHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  socket_.async_read_some(
      boost::asio::buffer(buffer_),
      [self](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, self->buffer_.data(),
            self->buffer_.data() + bytes_transferred);

        if (result == request_parser::good) {
          ByteRange vaddress((uint8_t *)self->buffer_.data(),
                             bytes_transferred);
          std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(vaddress);
          buf->trimStart(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          self->ProcessReceivedData(self, buf, error, buf->length());
        } else if (result == request_parser::bad) {
          self->OnDisconnect(error);
        }
      });
}

void Socks5Connection::ReadStream() {
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
          buf->append(bytes_transferred);
          ProcessReceivedData(self, buf, error, bytes_transferred);
          return 0;
        }
        return SOCKET_BUF_SIZE;
      });
}

void Socks5Connection::WriteMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  method_select_reply_ = method_select_response_stock_reply();
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
      std::bind(&Socks5Connection::ProcessSentData, self, nullptr,
                std::placeholders::_1, std::placeholders::_2));
}

void Socks5Connection::WriteHandshake(reply *reply) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  boost::asio::async_write(socket_, reply->buffers(),
                           std::bind(&Socks5Connection::ProcessSentData, self,
                                     nullptr, std::placeholders::_1,
                                     std::placeholders::_2));
}

void Socks5Connection::WriteStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  boost::asio::async_write(
      socket_, boost::asio::buffer(buf->data(), buf->length()),
      std::bind(&Socks5Connection::ProcessSentData, self, buf,
                std::placeholders::_1, std::placeholders::_2));
}

boost::system::error_code
Socks5Connection::PerformCmdOps(std::shared_ptr<Socks5Connection> self,
                                command_type command, reply *reply) {
  if (self->request_.address_type() == domain) {
    self->ss_request_ = std::make_unique<ss::request>(
        self->request_.domain_name(), self->request_.port());
  } else {
    self->ss_request_ =
        std::make_unique<ss::request>(self->request_.endpoint());
  }

  ByteRange vaddress(self->ss_request_->data(), self->ss_request_->length());
  switch (command) {
  case cmd_connect:
    return self->OnCmdConnect(vaddress, reply);
  case cmd_bind:
  case cmd_udp_associate:
  default:
    break;
  }
  return boost::system::errc::make_error_code(boost::system::errc::bad_message);
}

void Socks5Connection::ProcessReceivedData(
    std::shared_ptr<Socks5Connection> self, std::shared_ptr<IOBuf> buf,
    boost::system::error_code error, size_t bytes_transferred) {
  self->rbytes_transferred_ += bytes_transferred;
  if (bytes_transferred) {
    VLOG(4) << "received request: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_method_select:
      self->WriteMethodSelect();
      break;
    case state_handshake:
      error = PerformCmdOps(self,
                            static_cast<command_type>(self->request_.command()),
                            &self->reply_);

      if (error) {
        self->reply_.mutable_status() = reply::request_failed_cmd_not_supported;
        self->reply_.set_endpoint(self->request_.endpoint());
      } else {
        boost::asio::ip::tcp::endpoint endpoint = self->request_.endpoint();
        self->reply_.mutable_status() = reply::request_granted;
        if (self->request_.address_type() == domain) {
          // TBD async resolve/prevent DNS leak
          // TBD fix AAAA record
          // Get a list of endpoints corresponding to the SOCKS 5 domain name.
          boost::asio::ip::tcp::resolver resolver(self->io_context_);
          auto endpoints =
              resolver.resolve(self->request_.domain_name(),
                               std::to_string(self->request_.port()), error);
          if (!error) {
            endpoint = endpoints->endpoint();
            LOG(WARNING) << "[dns] reply with endpoint: " << endpoint
                         << " for domain " << self->request_.domain_name();
          }
        }
        self->reply_.set_endpoint(endpoint);
      }
      self->WriteHandshake(&self->reply_);
      if (!buf->length()) {
        break;
      }
    case state_stream:
      if (bytes_transferred) {
        self->OnStreamRead(buf);
      }
      self->ReadStream(); // continously read
      break;
    case state_error:
      error = boost::system::errc::make_error_code(
          boost::system::errc::bad_message);
      break;
    };
  }
  if (error) {
    self->SetState(state_error);
    self->OnDisconnect(error);
  }
};

void Socks5Connection::ProcessSentData(std::shared_ptr<Socks5Connection> self,
                                       std::shared_ptr<IOBuf> buf,
                                       boost::system::error_code error,
                                       size_t bytes_transferred) {
  self->wbytes_transferred_ += bytes_transferred;

  if (bytes_transferred) {
    VLOG(4) << "Process sent data: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_method_select:
      self->SetState(state_handshake);
      self->ReadHandshake(); // read next state info
      break;
    case state_handshake:
      self->SetState(state_stream);
      self->OnConnect();
      self->ReadStream(); // read next state info
      break;
    case state_stream:
      self->OnStreamWrite(buf);
      break;
    case state_error:
      error = boost::system::errc::make_error_code(
          boost::system::errc::bad_message);
      break;
    }
  }

  if (error) {
    self->SetState(state_error);
    self->OnDisconnect(error);
  }
};

boost::system::error_code Socks5Connection::OnCmdConnect(ByteRange vaddress,
                                                         reply *reply) {
  reply->mutable_status() = reply::request_granted;

  // ensure the remote is connected prior to header write
  channel_->connect();
  // write variable address directly as ss header
  std::unique_ptr<IOBuf> buf = IOBuf::copyBuffer(vaddress);
  std::shared_ptr<IOBuf> sharedBuf{buf.release()};
  OnUpstreamWrite(sharedBuf);

  return boost::system::errc::make_error_code(boost::system::errc::success);
}

void Socks5Connection::OnConnect() {
  VLOG(2) << "socks5: established connection with: " << endpoint_;
}

void Socks5Connection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "socks5: read: " << buf->length() << " bytes.";
  OnUpstreamWrite(buf);
}

void Socks5Connection::OnStreamWrite(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "socks5: sent reply: " << buf->length() << " bytes.";
  downstream_writable_ = true;

  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();

  /* recursively send the remainings */
  OnDownstreamWriteFlush();
}

void Socks5Connection::OnDisconnect(boost::system::error_code error) {
  VLOG(2) << "socks5: lost connection with: " << endpoint_ << " due to "
          << error;
  close();
}

void Socks5Connection::OnDownstreamWriteFlush() { OnDownstreamWrite(nullptr); }

void Socks5Connection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    downstream_.push_back(buf);
  }
  if (!downstream_.empty() && downstream_writable_) {
    std::shared_ptr<IOBuf> buf = downstream_[0];
    downstream_writable_ = false;
    WriteStream(buf);
  }
}

void Socks5Connection::OnUpstreamWriteFlush() {
  std::shared_ptr<IOBuf> buf{IOBuf::create(0).release()};
  OnUpstreamWrite(buf);
}

void Socks5Connection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    std::shared_ptr<IOBuf> buf = upstream_[0];
    upstream_writable_ = false;

    buf = EncryptData(buf);
    channel_->start_write(buf);
  }
}

void Socks5Connection::connected() {
  VLOG(2) << "remote: established connection with: " << remote_endpoint_;
  channel_->start_read();
  OnDownstreamWriteFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: received reply: " << buf->length() << " bytes.";
  buf = DecryptData(buf);
  OnDownstreamWrite(buf);
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: sent reply: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty());
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();
}

void Socks5Connection::disconnected(boost::system::error_code error) {
  VLOG(2) << "upstream: lost connection with: " << remote_endpoint_;
  close();
}

std::shared_ptr<IOBuf>
Socks5Connection::DecryptData(std::shared_ptr<IOBuf> cipherbuf) {
  std::unique_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
#ifndef NDEBUG
  DumpHex("ERead->", cipherbuf.get());
#endif
  decoder_->decrypt(cipherbuf.get(), plainbuf);
#ifndef NDEBUG
  DumpHex("PRead->", plainbuf.get());
#endif
  std::shared_ptr<IOBuf> buf{plainbuf.release()};
  return buf;
}

std::shared_ptr<IOBuf>
Socks5Connection::EncryptData(std::shared_ptr<IOBuf> buf) {
  std::unique_ptr<IOBuf> cipherbuf = IOBuf::create(buf->length());
#ifndef NDEBUG
  DumpHex("PWrite->", buf.get());
#endif
  encoder_->encrypt(buf.get(), cipherbuf);
#ifndef NDEBUG
  DumpHex("EWrite->", cipherbuf.get());
#endif
  std::shared_ptr<IOBuf> sharedBuf{cipherbuf.release()};
  return sharedBuf;
}

} // namespace socks5
