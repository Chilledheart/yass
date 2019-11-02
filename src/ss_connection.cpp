//
// ss_connection.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "ss_connection.hpp"

#include "cipher.hpp"
#include "config.hpp"

extern cipher::cipher_method cipher_method;

namespace ss {

SsConnection::SsConnection(
    boost::asio::io_context &io_context,
    const boost::asio::ip::tcp::endpoint &remote_endpoint)
    : Connection(io_context, remote_endpoint), state_(),
      encoder_(new cipher("", FLAGS_password, cipher_method, true)),
      decoder_(new cipher("", FLAGS_password, cipher_method)) {}

SsConnection::~SsConnection() {}

void SsConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  upstream_writable_ = true;
  downstream_writable_ = true;
  ReadHandshake();
}

void SsConnection::close() {
  if (closed_) {
    return;
  }
  LOG(WARNING) << "disconnected with client at stage: "
               << SsConnection::state_to_str(CurrentState());
  boost::system::error_code ec;
  closed_ = true;
  socket_.close(ec);
  if (channel_) {
    channel_->close();
  }
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void SsConnection::ReadHandshake() {
  std::shared_ptr<SsConnection> self = shared_from_this();
  std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  cipherbuf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(cipherbuf->mutable_data(),
                                  cipherbuf->capacity()),
      [self, cipherbuf](boost::system::error_code error,
                        size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        cipherbuf->append(bytes_transferred);
        std::shared_ptr<IOBuf> buf = self->DecryptData(cipherbuf);

        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, buf->data(), buf->data() + bytes_transferred);

        if (result == request_parser::good) {
          buf->trimStart(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          ProcessReceivedData(self, buf, error, buf->length());
        } else if (result == request_parser::bad) {
          self->OnDisconnect(error);
        }
      });
}

void SsConnection::ReadStream() {
  std::shared_ptr<SsConnection> self = shared_from_this();
  std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  cipherbuf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(cipherbuf->mutable_data(),
                                  cipherbuf->capacity()),
      [self, cipherbuf](boost::system::error_code error,
                        std::size_t bytes_transferred) -> std::size_t {
        if (!error) {
          VLOG(4) << "remaining available " << self->socket_.available()
                  << " bytes transferred: " << bytes_transferred << " bytes.";

          cipherbuf->append(bytes_transferred);
          std::shared_ptr<IOBuf> buf = self->DecryptData(cipherbuf);

          ProcessReceivedData(self, buf, error, buf->length());
          return 0;
        }
        if (error) {
          ProcessReceivedData(self, nullptr, error, bytes_transferred);
          return 0;
        }
        return SOCKET_BUF_SIZE;
      });
}

void SsConnection::WriteStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<SsConnection> self = shared_from_this();
  boost::asio::async_write(
      socket_, boost::asio::buffer(buf->data(), buf->length()),
      std::bind(&SsConnection::ProcessSentData, self, buf,
                std::placeholders::_1, std::placeholders::_2));
}

void SsConnection::ProcessReceivedData(std::shared_ptr<SsConnection> self,
                                       std::shared_ptr<IOBuf> buf,
                                       boost::system::error_code error,
                                       size_t bytes_transferred) {
  self->rbytes_transferred_ += bytes_transferred;
  if (bytes_transferred) {
    VLOG(4) << "received request: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_handshake:
      if (self->request_.address_type() == domain) {
        // Get a list of endpoints corresponding to the SOCKS 5 domain name.
        boost::asio::ip::tcp::resolver resolver(self->io_context_);
        auto endpoints =
            resolver.resolve(self->request_.domain_name(),
                             std::to_string(self->request_.port()), error);
        if (!error) {
          self->remote_endpoint_ = endpoints->endpoint();
          LOG(WARNING) << "found address name: "
                       << self->request_.domain_name();
        }
      } else {
        self->remote_endpoint_ = self->request_.endpoint();
      }
      self->SetState(state_stream);
      self->OnConnect();
      if (!buf->length()) {
        self->ReadStream(); // read next state info
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

void SsConnection::ProcessSentData(std::shared_ptr<SsConnection> self,
                                   std::shared_ptr<IOBuf> buf,
                                   boost::system::error_code error,
                                   size_t bytes_transferred) {
  self->wbytes_transferred_ += bytes_transferred;

  if (bytes_transferred) {
    VLOG(4) << "Process sent data: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_stream:
      self->OnStreamWrite(buf);
      break;
    case state_handshake:
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

void SsConnection::OnConnect() {
  VLOG(2) << "ss: established connection with: " << endpoint_
          << " remote: " << remote_endpoint_;
  channel_ = std::make_unique<ss::stream>(
      io_context_, remote_endpoint_,
      std::static_pointer_cast<Channel>(shared_from_this()));
  channel_->connect();
}

void SsConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "ss: read: " << buf->length() << " bytes.";
  OnUpstreamWrite(buf);
}

void SsConnection::OnStreamWrite(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "ss: sent reply: " << buf->length() << " bytes.";
  downstream_writable_ = true;

  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();

  /* recursively send the remainings */
  OnDownstreamWriteFlush();
}

void SsConnection::OnDisconnect(boost::system::error_code error) {
  VLOG(2) << "ss: lost connection with: " << endpoint_ << " due to " << error;
  close();
}

void SsConnection::OnDownstreamWriteFlush() { OnDownstreamWrite(nullptr); }

void SsConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    buf = EncryptData(buf);
    downstream_.push_back(buf);
  }
  if (!downstream_.empty() && downstream_writable_) {
    std::shared_ptr<IOBuf> buf = downstream_[0];
    downstream_writable_ = false;
    WriteStream(buf);
  }
}

void SsConnection::OnUpstreamWriteFlush() {
  std::shared_ptr<IOBuf> buf{IOBuf::create(0).release()};
  OnUpstreamWrite(buf);
}

void SsConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    std::shared_ptr<IOBuf> buf = upstream_[0];
    upstream_writable_ = false;

    channel_->start_write(buf);
  }
}

void SsConnection::connected() {
  VLOG(2) << "remote: established connection with: " << remote_endpoint_;
  channel_->start_read();
  OnDownstreamWriteFlush();
}

void SsConnection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: received reply: " << buf->length() << " bytes.";
  OnDownstreamWrite(buf);
}

void SsConnection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(4) << "upstream: sent reply: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty());
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();
}

void SsConnection::disconnected(boost::system::error_code error) {
  VLOG(2) << "upstream: lost connection with: " << remote_endpoint_
          << " due to " << error;
  close();
}

std::shared_ptr<IOBuf>
SsConnection::DecryptData(std::shared_ptr<IOBuf> cipherbuf) {
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

std::shared_ptr<IOBuf> SsConnection::EncryptData(std::shared_ptr<IOBuf> buf) {
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

} // namespace ss
