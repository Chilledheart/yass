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
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        buf->append(bytes_transferred);
        DumpHex("METHOD_SELECT->", buf.get());
        error = self->OnReadSocks5MethodSelect(buf);
        if (error) {
          error = self->OnReadSocks4Handshake(buf);
        }
        if (error) {
          self->OnDisconnect(error);
        }
      });
}

void Socks5Connection::ReadHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](boost::system::error_code error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        buf->append(bytes_transferred);
        DumpHex("HANDSHAKE->", buf.get());
        error = self->OnReadSocks5Handshake(buf);
        if (error) {
          self->OnDisconnect(error);
        }
      });
}

boost::system::error_code
Socks5Connection::OnReadSocks5MethodSelect(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  method_select_request_parser::result_type result;
  std::tie(result, std::ignore) = method_select_request_parser_.parse(
      method_select_request_, buf->data(), buf->data() + buf->length());

  if (result == method_select_request_parser::good) {
    DCHECK_LE(method_select_request_.length(), buf->length());
    buf->trimStart(method_select_request_.length());
    buf->retreat(method_select_request_.length());
    SetState(state_method_select);

    auto error =
        boost::system::errc::make_error_code(boost::system::errc::success);

    VLOG(2) << "client: socks5 method select";
    ProcessReceivedData(self, buf, error, buf->length());
    return error;
  }
  return boost::system::errc::make_error_code(boost::system::errc::bad_message);
}

boost::system::error_code
Socks5Connection::OnReadSocks5Handshake(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  request_parser::result_type result;
  std::tie(result, std::ignore) =
      request_parser_.parse(request_, buf->data(), buf->data() + buf->length());

  if (result == request_parser::good) {
    DCHECK_LE(request_.length(), buf->length());
    buf->trimStart(request_.length());
    buf->retreat(request_.length());
    SetState(state_handshake);

    auto error =
        boost::system::errc::make_error_code(boost::system::errc::success);

    VLOG(2) << "client: socks5 handshake";
    self->ProcessReceivedData(self, buf, error, buf->length());
    return error;
  }
  return boost::system::errc::make_error_code(boost::system::errc::bad_message);
}

boost::system::error_code
Socks5Connection::OnReadSocks4Handshake(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();

  socks4::request_parser::result_type result;
  std::tie(result, std::ignore) = s4_request_parser_.parse(
      s4_request_, buf->data(), buf->data() + buf->length());
  if (result == socks4::request_parser::good) {
    DCHECK_LE(s4_request_.length(), buf->length());
    buf->trimStart(s4_request_.length());
    buf->retreat(s4_request_.length());
    SetState(state_socks4_handshake);

    auto error =
        boost::system::errc::make_error_code(boost::system::errc::success);

    VLOG(2) << "client: socks4 handshake";
    ProcessReceivedData(self, buf, error, buf->length());
    return error;
  }
  return boost::system::errc::make_error_code(boost::system::errc::bad_message);
}

void Socks5Connection::ReadStream() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      boost::asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](const boost::system::error_code &error,
                  std::size_t bytes_transferred) -> std::size_t {
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

void Socks5Connection::WriteHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  if (CurrentState() == state_handshake) {
    boost::asio::async_write(socket_, reply_.buffers(),
                             std::bind(&Socks5Connection::ProcessSentData, self,
                                       nullptr, std::placeholders::_1,
                                       std::placeholders::_2));
  } else {
    boost::asio::async_write(socket_, s4_reply_.buffers(),
                             std::bind(&Socks5Connection::ProcessSentData, self,
                                       nullptr, std::placeholders::_1,
                                       std::placeholders::_2));
  }
}

void Socks5Connection::WriteStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  boost::asio::async_write(
      socket_, boost::asio::buffer(buf->data(), buf->length()),
      std::bind(&Socks5Connection::ProcessSentData, self, buf,
                std::placeholders::_1, std::placeholders::_2));
}

boost::system::error_code
Socks5Connection::PerformCmdOps(const socks5::request *request,
                                socks5::reply *reply) {
  if (request->address_type() == domain) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  boost::system::error_code error;
  error = boost::system::errc::make_error_code(boost::system::errc::success);

  switch (request->command()) {
  case socks5::cmd_connect: {
    boost::asio::ip::tcp::endpoint endpoint;
    if (request->address_type() == domain) {
      boost::asio::ip::tcp::resolver resolver(io_context_);
      auto endpoints = resolver.resolve(request->domain_name(),
                                        std::to_string(request->port()), error);
      if (!error) {
        endpoint = endpoints->endpoint();
        LOG(INFO) << "[dns] reply with endpoint: " << endpoint << " for domain "
                  << request->domain_name();
      } else {
        LOG(WARNING) << "[dns] resolve failure for domain "
                     << request->domain_name();
      }
    } else {
      endpoint = request->endpoint();
    }

    if (error) {
      reply->mutable_status() = socks5::reply::request_failed;
    } else {
      reply->set_endpoint(endpoint);
      reply->mutable_status() = socks5::reply::request_granted;
    }

    ByteRange req(ss_request_->data(), ss_request_->length());
    OnCmdConnect(req);
  } break;
  case socks5::cmd_bind:
  case socks5::cmd_udp_associate:
  default:
    // NOT IMPLETMENTED
    reply->mutable_status() = socks5::reply::request_failed_cmd_not_supported;
    break;
  }
  return error;
}

boost::system::error_code
Socks5Connection::PerformCmdOpsV4(const socks4::request *request,
                                  socks4::reply *reply) {
  if (request->is_socks4a()) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  boost::system::error_code error;
  error = boost::system::errc::make_error_code(boost::system::errc::success);

  switch (request->command()) {
  case socks4::cmd_connect: {
    boost::asio::ip::tcp::endpoint endpoint;

    if (request->is_socks4a()) {
      boost::asio::ip::tcp::resolver resolver(io_context_);
      auto endpoints = resolver.resolve(request->domain_name(),
                                        std::to_string(request->port()), error);
      if (!error) {
        endpoint = endpoints->endpoint();
        LOG(INFO) << "[dns] reply with endpoint: " << endpoint << " for domain "
                  << request->domain_name();
      } else {
        LOG(WARNING) << "[dns] resolve failure for domain "
                     << request->domain_name();
      }
    }

    if (error) {
      reply->mutable_status() = socks4::reply::request_failed;
    } else {
      reply->set_endpoint(endpoint);
      reply->mutable_status() = socks4::reply::request_granted;
    }

    ByteRange req(ss_request_->data(), ss_request_->length());
    OnCmdConnect(req);
  } break;
  case socks4::cmd_bind:
  default:
    // NOT IMPLETMENTED
    reply->mutable_status() = socks4::reply::request_failed;
    break;
  }
  return boost::system::errc::make_error_code(boost::system::errc::success);
}

void Socks5Connection::ProcessReceivedData(
    std::shared_ptr<Socks5Connection> self, std::shared_ptr<IOBuf> buf,
    boost::system::error_code error, size_t bytes_transferred) {
  self->rbytes_transferred_ += bytes_transferred;
  if (bytes_transferred) {
    VLOG(2) << "client: received request: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_method_select:
      self->WriteMethodSelect();
      self->SetState(state_handshake);
      break;
    case state_handshake:
      error = self->PerformCmdOps(&self->request_, &self->reply_);

      self->WriteHandshake();
      self->SetState(state_stream);
      if (buf->length()) {
        self->ProcessReceivedData(self, buf, error, buf->length());
      }
      break;
    case state_socks4_handshake:
      error = self->PerformCmdOpsV4(&self->s4_request_, &self->s4_reply_);

      self->WriteHandshake();
      self->SetState(state_stream);
      if (buf->length()) {
        self->ProcessReceivedData(self, buf, error, buf->length());
      }
      break;
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
    VLOG(2) << "client: sent data: " << bytes_transferred << " bytes.";
  }

  if (!error) {
    switch (self->CurrentState()) {
    case state_handshake:
      self->ReadHandshake(); // read next state info
      break;
    case state_stream:
      self->ReadStream(); // read next state info
      if (buf) {
        self->OnStreamWrite(buf);
      }
      break;
    case state_socks4_handshake:
    case state_method_select: // impossible
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

void Socks5Connection::OnCmdConnect(ByteRange req) {
  OnConnect();

  // ensure the remote is connected prior to header write
  channel_->connect();
  // write variable address directly as ss header
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
  OnUpstreamWrite(buf);
}

void Socks5Connection::OnConnect() {
  VLOG(2) << "client: established connection with: " << endpoint_;
}

void Socks5Connection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  OnUpstreamWrite(buf);
}

void Socks5Connection::OnStreamWrite(std::shared_ptr<IOBuf> buf) {
  downstream_writable_ = true;

  DCHECK(!downstream_.empty() && downstream_[0] == buf);
  downstream_.pop_front();

  /* recursively send the remainings */
  OnDownstreamWriteFlush();
}

void Socks5Connection::OnDisconnect(boost::system::error_code error) {
  VLOG(2) << "client: lost connection with: " << endpoint_ << " due to "
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
    buf = EncryptData(buf);
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    std::shared_ptr<IOBuf> buf = upstream_[0];
    upstream_writable_ = false;
    channel_->start_write(buf);
  }
}

void Socks5Connection::connected() {
  VLOG(1) << "remote: established connection with: " << remote_endpoint_;
  channel_->start_read();
  OnDownstreamWriteFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(2) << "upstream: received reply: " << buf->length() << " bytes.";
  buf = DecryptData(buf);
  OnDownstreamWrite(buf);
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(2) << "upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();
}

void Socks5Connection::disconnected(boost::system::error_code error) {
  VLOG(1) << "upstream: lost connection with: " << remote_endpoint_
          << " due to " << error;
  close();
}

std::shared_ptr<IOBuf>
Socks5Connection::DecryptData(std::shared_ptr<IOBuf> cipherbuf) {
  std::unique_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
  DumpHex("ERead->", cipherbuf.get());
  decoder_->decrypt(cipherbuf.get(), plainbuf);
  DumpHex("PRead->", plainbuf.get());
  std::shared_ptr<IOBuf> buf{plainbuf.release()};
  return buf;
}

std::shared_ptr<IOBuf>
Socks5Connection::EncryptData(std::shared_ptr<IOBuf> buf) {
  std::unique_ptr<IOBuf> cipherbuf = IOBuf::create(buf->length());
  DumpHex("PWrite->", buf.get());
  encoder_->encrypt(buf.get(), cipherbuf);
  DumpHex("EWrite->", cipherbuf.get());
  std::shared_ptr<IOBuf> sharedBuf{cipherbuf.release()};
  return sharedBuf;
}

} // namespace socks5
