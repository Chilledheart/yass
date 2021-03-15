// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "cli/socks5_connection.hpp"

#include <asio/error_code.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include "config/config.hpp"
#include "core/cipher.hpp"

static int http_request_url_parse(const char *buf, size_t len,
                                  std::string *host, uint16_t *port,
                                  int is_connect) {
  struct http_parser_url url;

  if (0 != ::http_parser_parse_url(buf, len, is_connect, &url)) {
    LOG(ERROR) << "Failed to parse url: '" << std::string(buf, len) << "'";
    return 1;
  }

  if (url.field_set & (1 << (UF_HOST))) {
    *host = std::string(buf + url.field_data[UF_HOST].off,
                        url.field_data[UF_HOST].len);
  }

  if (url.field_set & (1 << (UF_PORT))) {
    *port = url.port;
  }

  return 0;
}

// Convert plain http proxy header to http request header
//
// reforge HTTP Request Header and pretend it to buf
// including removal of Proxy-Connection header
static void http_request_reforge_to_bytes(
    std::string *header, ::http_parser *p, const std::string &url,
    const std::unordered_map<std::string, std::string> &headers) {
  std::stringstream ss;
  ss << http_method_str((http_method)p->method) << " " << url
     << " HTTP/1.1\r\n";
  for (const std::pair<std::string, std::string> pair : headers) {
    if (pair.first == "Proxy-Connection") {
      continue;
    }
    ss << pair.first << ": " << pair.second << "\r\n";
  }
  ss << "\r\n";

  *header = ss.str();
}

namespace socks5 {

Socks5Connection::Socks5Connection(
    asio::io_context &io_context,
    const asio::ip::tcp::endpoint &remote_endpoint)
    : Connection(io_context, remote_endpoint), state_(),
      encoder_(new cipher("", FLAGS_password, cipher_method_in_use, true)),
      decoder_(new cipher("", FLAGS_password, cipher_method_in_use)) {}

Socks5Connection::~Socks5Connection() {}

void Socks5Connection::start() {
  channel_ = std::make_unique<stream>(
      io_context_, remote_endpoint_,
      std::static_pointer_cast<Channel>(shared_from_this()));
  SetState(state_method_select);
  closed_ = false;
  upstream_writable_ = false;
  downstream_writable_ = true;
  ReadMethodSelect();
}

void Socks5Connection::close() {
  if (closed_) {
    return;
  }
  VLOG(2) << "disconnected with client at stage: "
          << Socks5Connection::state_to_str(CurrentState());
  asio::error_code ec;
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
      asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](const asio::error_code &error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        asio::error_code ec;
        buf->append(bytes_transferred);
        DumpHex("METHOD_SELECT->", buf.get());

        ec = self->OnReadSocks5MethodSelect(buf);
        if (ec) {
          ec = self->OnReadSocks4Handshake(buf);
        }
        if (ec) {
          ec = self->OnReadHttpRequest(buf);
        }
        if (ec) {
          self->OnDisconnect(ec);
        } else {
          self->ProcessReceivedData(self, buf, ec, buf->length());
        }
      });
}

void Socks5Connection::ReadHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](const asio::error_code &error, size_t bytes_transferred) {
        if (error) {
          self->OnDisconnect(error);
          return;
        }
        asio::error_code ec;
        buf->append(bytes_transferred);
        DumpHex("HANDSHAKE->", buf.get());
        ec = self->OnReadSocks5Handshake(buf);
        if (ec) {
          self->OnDisconnect(ec);
        }
      });
}

asio::error_code
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

    auto error = asio::error_code();

    VLOG(3) << "client: socks5 method select";
    ProcessReceivedData(self, buf, error, buf->length());
    return error;
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code
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

    auto error = asio::error_code();

    VLOG(3) << "client: socks5 handshake";
    return error;
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code
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

    auto error = asio::error_code();

    VLOG(3) << "client: socks4 handshake";
    return error;
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code
Socks5Connection::OnReadHttpRequest(std::shared_ptr<IOBuf> buf) {
  static struct http_parser_settings settings_connect = {
      //.on_message_begin
      nullptr,
      //.on_url
      &Socks5Connection::OnReadHttpRequestURL,
      //.on_status
      nullptr,
      //.on_header_field
      &Socks5Connection::OnReadHttpRequestHeaderField,
      //.on_header_value
      &Socks5Connection::OnReadHttpRequestHeaderValue,
      //.on_headers_complete
      Socks5Connection::OnReadHttpRequestHeadersDone,
      //.on_body
      nullptr,
      //.on_message_complete
      nullptr,
      //.on_chunk_header
      nullptr,
      //.on_chunk_complete
      nullptr};

  ::http_parser parser;
  size_t nparsed;
  ::http_parser_init(&parser, HTTP_REQUEST);

  parser.data = this;
  nparsed = http_parser_execute(&parser, &settings_connect,
                                (const char *)buf->data(), buf->length());
  if (nparsed) {
    VLOG(3) << "http: " << std::string((const char *)buf->data(), nparsed);
  }

  if (HTTP_PARSER_ERRNO(&parser) == HPE_OK) {
    buf->trimStart(nparsed);
    buf->retreat(nparsed);

    if (!http_is_connect_) {
      std::string header;
      http_request_reforge_to_bytes(&header, &parser, http_url_, http_headers_);
      buf->reserve(header.size(), 0);
      buf->prepend(header.size());
      memcpy(buf->mutable_data(), header.c_str(), header.size());
    }

    SetState(state_http_handshake);
    return asio::error_code();
  }

  LOG(WARNING) << http_errno_description(HTTP_PARSER_ERRNO(&parser)) << ": "
               << std::string((const char *)buf->data(), nparsed);
  return std::make_error_code(std::errc::bad_message);
}

int Socks5Connection::OnReadHttpRequestURL(http_parser *p, const char *buf,
                                           size_t len) {
  Socks5Connection *conn = reinterpret_cast<Socks5Connection *>(p->data);
  conn->http_url_ = std::string(buf, len);
  if (p->method == HTTP_CONNECT) {
    if (0 != http_request_url_parse(buf, len, &conn->http_host_,
                                    &conn->http_port_, 1)) {
      return 1;
    }
    conn->http_is_connect_ = true;
    VLOG(4) << "CONNECT: " << conn->http_host_ << " PORT: " << conn->http_port_;
  }
  return 0;
}

int Socks5Connection::OnReadHttpRequestHeaderField(http_parser *parser,
                                                   const char *buf,
                                                   size_t len) {
  Socks5Connection *conn = reinterpret_cast<Socks5Connection *>(parser->data);
  conn->http_field_ = std::string(buf, len);
  return 0;
}

int Socks5Connection::OnReadHttpRequestHeaderValue(http_parser *parser,
                                                   const char *buf,
                                                   size_t len) {
  Socks5Connection *conn = reinterpret_cast<Socks5Connection *>(parser->data);
  conn->http_value_ = std::string(buf, len);
  conn->http_headers_[conn->http_field_] = conn->http_value_;
  if (conn->http_field_ == "Host" && !conn->http_is_connect_) {
    const char *url = buf;
    // Host = "Host" ":" host [ ":" port ] ; Section 3.2.2
    // TBD hand with IPv6 address // [xxx]:port/xxx
    while (*buf != ':' && *buf != '\0' && len != 0) {
      buf++, len--;
    }

    conn->http_host_ = std::string(url, buf);
    if (len > 1 && *buf == ':') {
      ++buf, --len;
      conn->http_port_ = stoi(std::string(buf, len));
    } else {
      conn->http_port_ = 80;
    }

    VLOG(4) << "Host: " << conn->http_host_ << " PORT: " << conn->http_port_;
  }
  return 0;
}

int Socks5Connection::OnReadHttpRequestHeadersDone(http_parser *) {
  // Treat the rest part as Upgrade even when it is not CONNECT
  // (binary protocol such as ocsp-request and dns-message).
  return 2;
}

void Socks5Connection::ReadStream() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(
      asio::mutable_buffer(buf->mutable_data(), buf->capacity()),
      [self, buf](const asio::error_code &error,
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
  asio::async_write(
      socket_,
      asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
      std::bind(&Socks5Connection::ProcessSentData, self, nullptr,
                std::placeholders::_1, std::placeholders::_2));
}

void Socks5Connection::WriteHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  switch (CurrentState()) {
  case state_handshake:
    asio::async_write(socket_, reply_.buffers(),
                      std::bind(&Socks5Connection::ProcessSentData, self,
                                nullptr, std::placeholders::_1,
                                std::placeholders::_2));
    break;
  case state_socks4_handshake:
    asio::async_write(socket_, s4_reply_.buffers(),
                      std::bind(&Socks5Connection::ProcessSentData, self,
                                nullptr, std::placeholders::_1,
                                std::placeholders::_2));
    break;
  case state_http_handshake:
    /// reply on CONNECT request
    if (http_is_connect_) {
      asio::async_write(
          socket_,
          asio::buffer(http_connect_reply_.c_str(), http_connect_reply_.size()),
          std::bind(&Socks5Connection::ProcessSentData, self, nullptr,
                    std::placeholders::_1, std::placeholders::_2));
    }
    break;
  case state_method_select: // impossible
  case state_error:
  default:
    break;
    // bad
  }
}

void Socks5Connection::WriteStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  asio::async_write(socket_, asio::buffer(buf->data(), buf->length()),
                    std::bind(&Socks5Connection::ProcessSentData, self, buf,
                              std::placeholders::_1, std::placeholders::_2));
}

asio::error_code Socks5Connection::PerformCmdOps(const socks5::request *request,
                                                 socks5::reply *reply) {
  if (request->address_type() == domain) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  asio::error_code error;
  error = asio::error_code();

  switch (request->command()) {
  case socks5::cmd_connect: {
    asio::ip::tcp::endpoint endpoint;
    if (request->address_type() == domain) {
      endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);
    } else {
      endpoint = request->endpoint();
    }

    if (error) {
      reply->mutable_status() = socks5::reply::request_failed;
    } else {
      reply->set_endpoint(endpoint);
      // reply->set_loopback();
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

asio::error_code
Socks5Connection::PerformCmdOpsV4(const socks4::request *request,
                                  socks4::reply *reply) {
  if (request->is_socks4a()) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  asio::error_code error;
  error = asio::error_code();

  switch (request->command()) {
  case socks4::cmd_connect: {
    asio::ip::tcp::endpoint endpoint;

    if (request->is_socks4a()) {
      // TBD
    }
    endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);

    if (error) {
      reply->mutable_status() = socks4::reply::request_failed;
    } else {
      reply->set_endpoint(endpoint);
      // reply->set_loopback();
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
  return asio::error_code();
}

asio::error_code Socks5Connection::PerformCmdOpsHttp() {
  ss_request_ = std::make_unique<ss::request>(http_host_, http_port_);

  asio::error_code error;
  error = asio::error_code();

  asio::ip::tcp::endpoint endpoint;

  endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);

  ByteRange req(ss_request_->data(), ss_request_->length());
  OnCmdConnect(req);

  return asio::error_code();
}

void Socks5Connection::ProcessReceivedData(
    std::shared_ptr<Socks5Connection> self, std::shared_ptr<IOBuf> buf,
    const asio::error_code &error, size_t bytes_transferred) {
  self->rbytes_transferred_ += bytes_transferred;
  if (bytes_transferred) {
    VLOG(3) << "client: received request: " << bytes_transferred << " bytes.";
  }

  asio::error_code ec = error;

  if (!ec) {
    switch (self->CurrentState()) {
    case state_method_select:
      self->WriteMethodSelect();
      self->SetState(state_handshake);
      break;
    case state_handshake:
      ec = self->PerformCmdOps(&self->request_, &self->reply_);

      self->WriteHandshake();
      self->SetState(state_stream);
      if (buf->length()) {
        self->ProcessReceivedData(self, buf, ec, buf->length());
      }
      break;
    case state_socks4_handshake:
      ec = self->PerformCmdOpsV4(&self->s4_request_, &self->s4_reply_);

      self->WriteHandshake();
      self->SetState(state_stream);
      if (buf->length()) {
        self->ProcessReceivedData(self, buf, ec, buf->length());
      }
      break;
    case state_http_handshake:
      ec = self->PerformCmdOpsHttp();

      self->WriteHandshake();
      self->SetState(state_stream);
      if (buf->length()) {
        self->ProcessReceivedData(self, buf, ec, buf->length());
      }
      break;
    case state_stream:
      if (bytes_transferred) {
        self->OnStreamRead(buf);
      }
      self->ReadStream(); // continously read
      break;
    case state_error:
      ec = std::make_error_code(std::errc::bad_message);
      break;
    };
  }
  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
  }
};

void Socks5Connection::ProcessSentData(std::shared_ptr<Socks5Connection> self,
                                       std::shared_ptr<IOBuf> buf,
                                       const asio::error_code &error,
                                       size_t bytes_transferred) {
  self->wbytes_transferred_ += bytes_transferred;

  if (bytes_transferred) {
    VLOG(3) << "client: sent data: " << bytes_transferred << " bytes.";
  }

  asio::error_code ec = error;

  if (!ec) {
    switch (self->CurrentState()) {
    case state_handshake:
      self->ReadHandshake(); // read next state info
      break;
    case state_stream:
      if (buf) {
        self->OnStreamWrite(buf);
      } else {
        self->ReadStream(); // read next state info
      }
      break;
    case state_socks4_handshake:
    case state_method_select: // impossible
    case state_http_handshake:
    case state_error:
      ec = std::make_error_code(std::errc::bad_message);
      break;
    }
  }

  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
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

void Socks5Connection::OnDisconnect(asio::error_code error) {
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
    downstream_writable_ = false;
    WriteStream(downstream_[0]);
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
    upstream_writable_ = false;
    channel_->start_write(upstream_[0]);
  }
}

void Socks5Connection::connected() {
  VLOG(2) << "remote: established connection with: " << remote_endpoint_;
  upstream_writable_ = true;
  channel_->start_read();
  OnDownstreamWriteFlush();
  OnUpstreamWriteFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "upstream: received reply: " << buf->length() << " bytes.";
  buf = DecryptData(buf);
  OnDownstreamWrite(buf);
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();
}

void Socks5Connection::disconnected(asio::error_code error) {
  VLOG(2) << "upstream: lost connection with: " << remote_endpoint_
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
