//
// ss_connection.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "ss_connection.hpp"

#include "cipher.hpp"
#include "config.hpp"

#define MAX_DOWNSTREAM_DEPS 1024
#define MAX_UPSTREAM_DEPS 1024

extern cipher::cipher_method cipher_method;

namespace ss {

SsConnection::SsConnection(
    boost::asio::io_context &io_context,
    const boost::asio::ip::tcp::endpoint &remote_endpoint)
    : Connection(io_context, remote_endpoint), state_(), resolver_(io_context_),
      encoder_(new cipher("", FLAGS_password, cipher_method, true)),
      decoder_(new cipher("", FLAGS_password, cipher_method)) {}

SsConnection::~SsConnection() {}

void SsConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  upstream_writable_ = false;
  downstream_writable_ = true;
  downstream_readable_ = true;
  ReadHandshake();
}

void SsConnection::close() {
  if (closed_) {
    return;
  }
  VLOG(2) << "disconnected with client at stage: "
          << SsConnection::state_to_str(CurrentState());
  boost::system::error_code ec;
  closed_ = true;
  socket_.close(ec);
  if (channel_) {
    channel_->close();
  }
  resolver_.cancel();
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

        DumpHex("HANDSHAKE->", buf.get());

        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, buf->data(), buf->data() + bytes_transferred);

        if (result == request_parser::good) {
          buf->trimStart(self->request_.length());
          buf->retreat(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          ProcessReceivedData(self, buf, error, buf->length());
        } else {
          self->OnDisconnect(error);
        }
      });
}

void SsConnection::ResolveDns(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<SsConnection> self = shared_from_this();
  resolver_.async_resolve(
      self->request_.domain_name(), std::to_string(self->request_.port()),
      [self, buf](const boost::system::error_code &error,
                  boost::asio::ip::tcp::resolver::results_type results) {
        // Get a list of endpoints corresponding to the SOCKS 5 domain name.
        if (!error) {
          self->remote_endpoint_ = results->endpoint();
          VLOG(2) << "found address name: " << self->request_.domain_name();
          self->SetState(state_stream);
          self->OnConnect();
          ProcessReceivedData(self, buf, error, buf->length());
        } else {
          self->OnDisconnect(error);
        }
      });
}

void SsConnection::ReadStream() {
  std::shared_ptr<SsConnection> self = shared_from_this();
  std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  cipherbuf->reserve(0, SOCKET_BUF_SIZE);
  downstream_read_inprogress_ = true;

  socket_.async_read_some(
      boost::asio::mutable_buffer(cipherbuf->mutable_data(),
                                  cipherbuf->capacity()),
      [self, cipherbuf](boost::system::error_code error,
                        std::size_t bytes_transferred) -> std::size_t {
        if (bytes_transferred || error) {
          cipherbuf->append(bytes_transferred);
          std::shared_ptr<IOBuf> buf;
          if (!error) {
            buf = self->DecryptData(cipherbuf);
            bytes_transferred = buf->length();
          }
          self->downstream_read_inprogress_ = false;
          ProcessReceivedData(self, buf, error, bytes_transferred);
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
    VLOG(3) << "received request: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_handshake:
      if (self->request_.address_type() == domain) {
        self->ResolveDns(buf);
        return;
      }
      self->remote_endpoint_ = self->request_.endpoint();
      self->SetState(state_stream);
      self->OnConnect();
      DCHECK_EQ(buf->length(), bytes_transferred);
    case state_stream:
      if (bytes_transferred) {
        self->OnStreamRead(buf);
      }
      if (self->downstream_readable_) {
        self->ReadStream(); // continously read
      }
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
    VLOG(3) << "sent data: " << bytes_transferred << " bytes.";
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
  // queue limit to downstream read
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    DisableStreamRead();
  }

  OnUpstreamWrite(buf);
}

void SsConnection::OnStreamWrite(std::shared_ptr<IOBuf> buf) {
  downstream_writable_ = true;

  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();

  /* recursively send the remainings */
  OnDownstreamWriteFlush();

  /* close the socket if upstream is eof */
  if (!channel_->connected() && downstream_.empty()) {
    close();
    return;
  }

  /* disable queue limit to re-enable upstream read */
  if (downstream_.size() < MAX_DOWNSTREAM_DEPS && !upstream_readable_) {
    upstream_readable_ = true;
    channel_->enable_read();
  }
}

void SsConnection::EnableStreamRead() {
  if (!downstream_readable_) {
    downstream_readable_ = true;
    if (!downstream_read_inprogress_) {
      ReadStream();
    }
  }
}

void SsConnection::DisableStreamRead() { downstream_readable_ = false; }

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
    downstream_writable_ = false;
    WriteStream(downstream_[0]);
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
    upstream_writable_ = false;
    channel_->start_write(upstream_[0]);
  }
}

void SsConnection::connected() {
  VLOG(2) << "remote: established connection with: " << remote_endpoint_;
  upstream_readable_ = true;
  upstream_writable_ = true;
  channel_->start_read();
  OnUpstreamWriteFlush();
}

void SsConnection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "upstream: received reply: " << buf->length() << " bytes.";

  // queue limit to upstream read
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    upstream_readable_ = false;
    channel_->disable_read();
  }

  OnDownstreamWrite(buf);
}

void SsConnection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();

  if (upstream_.size() < MAX_UPSTREAM_DEPS && !downstream_readable_) {
    EnableStreamRead();
  }
}

void SsConnection::disconnected(boost::system::error_code error) {
  VLOG(2) << "upstream: lost connection with: " << remote_endpoint_
          << " due to " << error;
  upstream_writable_ = false;
  /* delay the socket's close because downstream is buffered */
  if (downstream_.empty()) {
    close();
  }
}

std::shared_ptr<IOBuf>
SsConnection::DecryptData(std::shared_ptr<IOBuf> cipherbuf) {
  std::unique_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
  DumpHex("ERead->", cipherbuf.get());
  decoder_->decrypt(cipherbuf.get(), plainbuf);
  DumpHex("PRead->", plainbuf.get());
  std::shared_ptr<IOBuf> buf{plainbuf.release()};
  return buf;
}

std::shared_ptr<IOBuf> SsConnection::EncryptData(std::shared_ptr<IOBuf> buf) {
  std::unique_ptr<IOBuf> cipherbuf = IOBuf::create(buf->length());
  DumpHex("PWrite->", buf.get());
  encoder_->encrypt(buf.get(), cipherbuf);
  DumpHex("EWrite->", cipherbuf.get());
  std::shared_ptr<IOBuf> sharedBuf{cipherbuf.release()};
  return sharedBuf;
}

} // namespace ss
