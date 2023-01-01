// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "cli/socks5_connection.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/cipher.hpp"

const char Socks5Connection::http_connect_reply_[] =
    "HTTP/1.1 200 Connection established\r\n\r\n";

#ifdef __linux__
#include <linux/netfilter_ipv4.h>
#endif

#ifdef __linux__
namespace {

bool IsIPv4MappedIPv6(const asio::ip::tcp::endpoint& address) {
  return address.address().is_v6() && address.address().to_v6().is_v4_mapped();
}

bool IsIPUnspecified(const asio::ip::tcp::endpoint& address) {
  return address.address().is_unspecified();
}

asio::ip::tcp::endpoint IPaddressFromSockAddr(struct sockaddr_storage* ss,
                                              socklen_t ss_len) {
  asio::ip::tcp::endpoint endpoint;

  if (ss_len == sizeof(sockaddr_in)) {
    auto socket = reinterpret_cast<struct sockaddr_in*>(ss);
    auto addr = asio::ip::address_v4(ntohl(socket->sin_addr.s_addr));
    endpoint.address(addr);
    endpoint.port(ntohs(socket->sin_port));
  } else if (ss_len == sizeof(sockaddr_in6)) {
    auto socket = reinterpret_cast<struct sockaddr_in6*>(ss);
    std::array<unsigned char, 16> bytes = {
      socket->sin6_addr.s6_addr[0],
      socket->sin6_addr.s6_addr[1],
      socket->sin6_addr.s6_addr[2],
      socket->sin6_addr.s6_addr[3],
      socket->sin6_addr.s6_addr[4],
      socket->sin6_addr.s6_addr[5],
      socket->sin6_addr.s6_addr[6],
      socket->sin6_addr.s6_addr[7],
      socket->sin6_addr.s6_addr[8],
      socket->sin6_addr.s6_addr[9],
      socket->sin6_addr.s6_addr[10],
      socket->sin6_addr.s6_addr[11],
      socket->sin6_addr.s6_addr[12],
      socket->sin6_addr.s6_addr[13],
      socket->sin6_addr.s6_addr[14],
      socket->sin6_addr.s6_addr[15]
    };
    auto addr = asio::ip::address_v6(bytes, socket->sin6_scope_id);
    endpoint.address(addr);
    endpoint.port(ntohs(socket->sin6_port));
  }
  return endpoint;
}

} // anonymous namespace
#endif

static int http_request_url_parse(const char* buf,
                                  size_t len,
                                  std::string* host,
                                  uint16_t* port,
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
    std::string* header,
    ::http_parser* p,
    const std::string& url,
    const absl::flat_hash_map<std::string, std::string>& headers) {
  std::stringstream ss;
  ss << http_method_str((http_method)p->method) << " "  // NOLINT(google-*)
     << url << " HTTP/1.1\r\n";
  for (const std::pair<std::string, std::string> pair : headers) {
    if (pair.first == "Proxy-Connection") {
      continue;
    }
    ss << pair.first << ": " << pair.second << "\r\n";
  }
  ss << "\r\n";

  *header = ss.str();
}

Socks5Connection::Socks5Connection(
    asio::io_context& io_context,
    const asio::ip::tcp::endpoint& remote_endpoint)
    : Connection(io_context, remote_endpoint),
      state_(),
      encoder_(new cipher(
          "",
          absl::GetFlag(FLAGS_password),
          static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
          true)),
      decoder_(new cipher("",
                          absl::GetFlag(FLAGS_password),
                          static_cast<enum cipher_method>(
                              absl::GetFlag(FLAGS_cipher_method)))) {}

Socks5Connection::~Socks5Connection() {
  VLOG(2) << "Connection (client) " << connection_id() << " freed memory";
};

void Socks5Connection::start() {
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
  VLOG(2) << "Connection (client) " << connection_id()
          << " disconnected with client at stage: "
          << Socks5Connection::state_to_str(CurrentState())
          << " and data to write: " << downstream_.size();
  asio::error_code ec;
  closed_ = true;
  socket_.cancel(ec);
  if (ec) {
    VLOG(2) << "cancel() error: " << ec;
  }
  socket_.close(ec);
  if (ec) {
    VLOG(2) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void Socks5Connection::ReadMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(mutable_buffer(*buf),
      [self, buf](asio::error_code ec, size_t bytes_transferred) {
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        buf->append(bytes_transferred);
        DumpHex("HANDSHAKE/METHOD_SELECT->", buf.get());

        ec = self->OnReadRedirHandshake(buf);
        if (ec) {
          ec = self->OnReadSocks5MethodSelect(buf);
        }
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

void Socks5Connection::ReadSocks5Handshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(mutable_buffer(*buf),
      [self, buf](asio::error_code ec, size_t bytes_transferred) {
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        buf->append(bytes_transferred);
        DumpHex("HANDSHAKE->", buf.get());
        ec = self->OnReadSocks5Handshake(buf);
        if (ec) {
          self->OnDisconnect(ec);
        } else {
          self->ProcessReceivedData(self, buf, ec, buf->length());
        }
      });
}

asio::error_code Socks5Connection::OnReadRedirHandshake(
    std::shared_ptr<IOBuf> buf) {
#ifdef __linux__
  VLOG(3) << "Connection (client) " << connection_id()
          << " try redir handshake";
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  auto peer_address = socket_.remote_endpoint();
  struct sockaddr_storage ss = {};
  socklen_t ss_len = sizeof(ss);
  asio::ip::tcp::endpoint endpoint;
  int ret;
  if (peer_address.address().is_v4() || IsIPv4MappedIPv6(peer_address))
    ret = getsockopt(socket_.native_handle(), SOL_IP, SO_ORIGINAL_DST, &ss,
                     &ss_len);
  else
    ret = getsockopt(socket_.native_handle(), SOL_IPV6, SO_ORIGINAL_DST, &ss,
                     &ss_len);
  if (ret == 0) {
    endpoint = IPaddressFromSockAddr(&ss, ss_len);
  }
  if (ret == 0 && !IsIPUnspecified(endpoint)) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " redir stream from " << endpoint_ << " to " << endpoint;

    // no handshake required to be written
    SetState(state_stream);

    ss_request_ = std::make_unique<ss::request>(endpoint);
    ByteRange req(ss_request_->data(), ss_request_->length());
    OnCmdConnect(IOBuf::copyBuffer(req));

    if (buf->length()) {
      ProcessReceivedData(self, buf, asio::error_code(), buf->length());
    } else {
      ReadStream();  // continously read
    }
    return asio::error_code();
  }
#endif
  return std::make_error_code(std::errc::network_unreachable);
}

asio::error_code Socks5Connection::OnReadSocks5MethodSelect(
    std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  socks5::method_select_request_parser::result_type result;
  std::tie(result, std::ignore) = method_select_request_parser_.parse(
      method_select_request_, buf->data(), buf->data() + buf->length());

  if (result == socks5::method_select_request_parser::good) {
    DCHECK_LE(method_select_request_.length(), buf->length());
    buf->trimStart(method_select_request_.length());
    buf->retreat(method_select_request_.length());
    SetState(state_method_select);

    VLOG(3) << "Connection (client) " << connection_id()
            << " socks5 method select";
    return asio::error_code();
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code Socks5Connection::OnReadSocks5Handshake(
    std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " try socks5 handshake";
  socks5::request_parser::result_type result;
  std::tie(result, std::ignore) = request_parser_.parse(
      s5_request_, buf->data(), buf->data() + buf->length());

  if (result == socks5::request_parser::good) {
    DCHECK_LE(s5_request_.length(), buf->length());
    buf->trimStart(s5_request_.length());
    buf->retreat(s5_request_.length());
    SetState(state_socks5_handshake);

    VLOG(3) << "Connection (client) " << connection_id()
            << " socks5 handshake began";
    return asio::error_code();
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code Socks5Connection::OnReadSocks4Handshake(
    std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " try socks4 handshake";

  socks4::request_parser::result_type result;
  std::tie(result, std::ignore) = s4_request_parser_.parse(
      s4_request_, buf->data(), buf->data() + buf->length());
  if (result == socks4::request_parser::good) {
    DCHECK_LE(s4_request_.length(), buf->length());
    buf->trimStart(s4_request_.length());
    buf->retreat(s4_request_.length());
    SetState(state_socks4_handshake);

    VLOG(3) << "Connection (client) " << connection_id()
            << " socks4 handshake began";
    return asio::error_code();
  }
  return std::make_error_code(std::errc::bad_message);
}

asio::error_code Socks5Connection::OnReadHttpRequest(
    std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " try http handshake";

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
                                reinterpret_cast<const char*>(buf->data()),
                                buf->length());
  if (nparsed) {
    VLOG(4) << "Connection (client) " << connection_id()
            << " http: "
            << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
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
    VLOG(3) << "Connection (client) " << connection_id()
            << " http handshake began";
    return asio::error_code();
  }

  LOG(WARNING) << "Connection (client) " << connection_id()
               << http_errno_description(HTTP_PARSER_ERRNO(&parser)) << ": "
               << std::string(reinterpret_cast<const char*>(buf->data()),
                              nparsed);
  return std::make_error_code(std::errc::bad_message);
}

int Socks5Connection::OnReadHttpRequestURL(http_parser* p,
                                           const char* buf,
                                           size_t len) {
  Socks5Connection* conn = reinterpret_cast<Socks5Connection*>(p->data);
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

int Socks5Connection::OnReadHttpRequestHeaderField(http_parser* parser,
                                                   const char* buf,
                                                   size_t len) {
  Socks5Connection* conn = reinterpret_cast<Socks5Connection*>(parser->data);
  conn->http_field_ = std::string(buf, len);
  return 0;
}

int Socks5Connection::OnReadHttpRequestHeaderValue(http_parser* parser,
                                                   const char* buf,
                                                   size_t len) {
  Socks5Connection* conn = reinterpret_cast<Socks5Connection*>(parser->data);
  conn->http_value_ = std::string(buf, len);
  conn->http_headers_[conn->http_field_] = conn->http_value_;
  if (conn->http_field_ == "Host" && !conn->http_is_connect_) {
    const char* url = buf;
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

int Socks5Connection::OnReadHttpRequestHeadersDone(http_parser*) {
  // Treat the rest part as Upgrade even when it is not CONNECT
  // (binary protocol such as ocsp-request and dns-message).
  return 2;
}

void Socks5Connection::ReadStream() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);

  socket_.async_read_some(mutable_buffer(*buf),
      [self, buf](asio::error_code ec,
                  std::size_t bytes_transferred) -> std::size_t {
        if (bytes_transferred || ec) {
          buf->append(bytes_transferred);
          ProcessReceivedData(self, buf, ec, bytes_transferred);
          return 0;
        }
        return SOCKET_BUF_SIZE;
      });
}

void Socks5Connection::WriteMethodSelect() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  method_select_reply_ = socks5::method_select_response_stock_reply();
  asio::async_write(
      socket_,
      asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
      [self](asio::error_code error, size_t bytes_transferred) {
        return Socks5Connection::ProcessSentData(self, nullptr, error,
                                                 bytes_transferred);
      });
}

void Socks5Connection::WriteHandshake() {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  switch (CurrentState()) {
    case state_method_select:  // impossible
    case state_socks5_handshake:
      self->SetState(state_stream);
      asio::async_write(
          socket_, s5_reply_.buffers(),
          [self](asio::error_code error, size_t bytes_transferred) {
            return Socks5Connection::ProcessSentData(self, nullptr, error,
                                                     bytes_transferred);
          });
      break;
    case state_socks4_handshake:
      self->SetState(state_stream);
      asio::async_write(
          socket_, s4_reply_.buffers(),
          [self](asio::error_code error, size_t bytes_transferred) {
            return Socks5Connection::ProcessSentData(self, nullptr, error,
                                                     bytes_transferred);
          });
      break;
    case state_http_handshake:
      self->SetState(state_stream);
      /// reply on CONNECT request
      if (http_is_connect_) {
        std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(
            http_connect_reply_, sizeof(http_connect_reply_) - 1);
        OnDownstreamWrite(buf);
      }
      break;
    case state_error:
      break;
    case state_stream:
      break;
    default:
      LOG(FATAL) << "Connection (client) " << connection_id()
                 << "bad state 0x" << std::hex
                 << static_cast<int>(self->CurrentState()) << std::dec;
  }
}

void Socks5Connection::WriteStream(std::shared_ptr<IOBuf> buf) {
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  DCHECK(downstream_writable_);
  downstream_writable_ = false;
  DCHECK_EQ(CurrentState(), state_stream);
  DCHECK(buf);
  asio::async_write(socket_, const_buffer(*buf),
      [self, buf](asio::error_code error, size_t bytes_transferred) {
        return Socks5Connection::ProcessSentData(self, buf, error,
                                                 bytes_transferred);
      });
}

asio::error_code Socks5Connection::PerformCmdOpsV5(
    const socks5::request* request,
    socks5::reply* reply) {
  if (request->address_type() == socks5::domain) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  asio::error_code error;

  switch (request->command()) {
    case socks5::cmd_connect: {
      asio::ip::tcp::endpoint endpoint;
      if (request->address_type() == socks5::domain) {
        endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);
      } else {
        endpoint = request->endpoint();
      }
      reply->set_endpoint(endpoint);
      reply->mutable_status() = socks5::reply::request_granted;

      ByteRange req(ss_request_->data(), ss_request_->length());
      OnCmdConnect(IOBuf::copyBuffer(req));
    } break;
    case socks5::cmd_bind:
    case socks5::cmd_udp_associate:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks5::reply::request_failed_cmd_not_supported;
      error = asio::error::invalid_argument;
      break;
  }
  return error;
}

asio::error_code Socks5Connection::PerformCmdOpsV4(
    const socks4::request* request,
    socks4::reply* reply) {
  if (request->is_socks4a()) {
    ss_request_ =
        std::make_unique<ss::request>(request->domain_name(), request->port());
  } else {
    ss_request_ = std::make_unique<ss::request>(request->endpoint());
  }

  asio::error_code error;

  switch (request->command()) {
    case socks4::cmd_connect: {
      asio::ip::tcp::endpoint endpoint;

      if (request->is_socks4a()) {
        // TBD
        LOG(WARNING) << "Connection (client) " << connection_id()
                     << " not supported protocol socks4a";
        error = asio::error::invalid_argument;
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
      OnCmdConnect(IOBuf::copyBuffer(req));
    } break;
    case socks4::cmd_bind:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks4::reply::request_failed;
      error = asio::error::invalid_argument;
      break;
  }
  return error;
}

asio::error_code Socks5Connection::PerformCmdOpsHttp() {
  ss_request_ = std::make_unique<ss::request>(http_host_, http_port_);

  asio::error_code error;

  ByteRange req(ss_request_->data(), ss_request_->length());
  OnCmdConnect(IOBuf::copyBuffer(req));

  return asio::error_code();
}

void Socks5Connection::ProcessReceivedData(
    std::shared_ptr<Socks5Connection> self,
    std::shared_ptr<IOBuf> buf,
    asio::error_code ec,
    size_t bytes_transferred) {

  VLOG(3) << "Connection (client) " << self->connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " ec: " << ec;

  self->rbytes_transferred_ += bytes_transferred;
  total_rx_bytes += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (self->CurrentState()) {
      case state_method_select:
        self->WriteMethodSelect();
        break;
      case state_socks5_handshake:
        ec = self->PerformCmdOpsV5(&self->s5_request_, &self->s5_reply_);
        self->WriteHandshake();
        VLOG(3) << "Connection (client) " << self->connection_id()
                << " socks5 handshake finished";
        if (buf->length()) {
          self->ProcessReceivedData(self, buf, ec, buf->length());
        } else {
          self->ReadStream();  // continously read
        }
        break;
      case state_socks4_handshake:
        ec = self->PerformCmdOpsV4(&self->s4_request_, &self->s4_reply_);
        self->WriteHandshake();
        VLOG(3) << "Connection (client) " << self->connection_id()
                << " socks4 handshake finished";
        if (buf->length()) {
          self->ProcessReceivedData(self, buf, ec, buf->length());
        } else {
          self->ReadStream();  // continously read
        }
        break;
      case state_http_handshake:
        ec = self->PerformCmdOpsHttp();
        self->WriteHandshake();
        VLOG(3) << "Connection (client) " << self->connection_id()
                << " http handshake finished";
        if (buf->length()) {
          self->ProcessReceivedData(self, buf, ec, buf->length());
        } else {
          self->ReadStream();  // continously read
        }
        break;
      case state_stream:
        DCHECK_NE(bytes_transferred, 0u);
        self->OnStreamRead(buf);
        self->ReadStream();  // continously read
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << self->connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(self->CurrentState()) << std::dec;
    };
  }
#if 1
  // Silence Read EOF error triggered by upstream disconnection
  if (ec == asio::error::eof && self->channel_ && self->channel_->eof()) {
    return;
  }
#endif
  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
  }
};

void Socks5Connection::ProcessSentData(std::shared_ptr<Socks5Connection> self,
                                       std::shared_ptr<IOBuf> buf,
                                       asio::error_code ec,
                                       size_t bytes_transferred) {

  VLOG(3) << "Connection (client) " << self->connection_id()
          << " sent data: " << bytes_transferred << " bytes"
          << " ec: " << ec << " and data to write: " << self->downstream_.size();

  self->wbytes_transferred_ += bytes_transferred;
  total_tx_bytes += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (self->CurrentState()) {
      case state_method_select:
        self->ReadSocks5Handshake();  // read next state info
        break;
      case state_socks5_handshake:
      case state_socks4_handshake:
      case state_http_handshake:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      case state_stream:
        self->OnStreamWrite(buf);
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << self->connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(self->CurrentState()) << std::dec;
    }
  }

  if (ec) {
    self->SetState(state_error);
    self->OnDisconnect(ec);
  }
};

void Socks5Connection::OnCmdConnect(std::shared_ptr<IOBuf> buf) {
  OnConnect();
  // ensure the remote is connected prior to header write
  channel_->connect();
  // write variable address directly as ss header
  OnUpstreamWrite(buf);
}

void Socks5Connection::OnConnect() {
  VLOG(2) << "Connection (client) " << connection_id()
          << " client: established connection with: " << endpoint_;
  std::shared_ptr<Socks5Connection> self = shared_from_this();
  // create lazy
  channel_ = std::make_unique<stream>(*io_context_, remote_endpoint_,
                                      std::static_pointer_cast<Channel>(self));
  upstream_writable_ = false;
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

  /* shutdown the socket if upstream is eof and all remaining data sent */
  if (channel_->eof() && downstream_.empty()) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " last data sent: shutting down";
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    return;
  }
}

void Socks5Connection::OnDisconnect(asio::error_code ec) {
  VLOG(2) << "Connection (client) " << connection_id()
          << " lost connection with: " << endpoint_ << " due to "
          << ec << " and data to write: " << downstream_.size();
  close();
}

void Socks5Connection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void Socks5Connection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf) {
    DCHECK(!buf->empty());
    downstream_.push_back(buf);
  }
  if (!downstream_.empty() && downstream_writable_) {
    WriteStream(downstream_[0]);
  } else {
    VLOG(3) << "Connection (client) " << connection_id()
            << " waiting for downstream writable in-progress: "
            << downstream_.size();
  }
}

void Socks5Connection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void Socks5Connection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " upstream: ready to send request: " << buf->length() << " bytes.";
    upstream_.push_back(EncryptData(buf));
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    channel_->start_write(upstream_[0]);
  }
}

void Socks5Connection::connected() {
  VLOG(3) << "Connection (client) " << connection_id()
          << " remote: established connection with: " << remote_endpoint_;
  upstream_writable_ = true;
  channel_->start_read();
  OnDownstreamWriteFlush();
  OnUpstreamWriteFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " upstream: received reply: " << buf->length() << " bytes.";
  buf = DecryptData(buf);
  if (!buf->empty()) {
    OnDownstreamWrite(buf);
  }
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  /* recursively send the remainings */
  upstream_writable_ = true;
  OnUpstreamWriteFlush();
}

void Socks5Connection::disconnected(asio::error_code ec) {
  VLOG(2) << "Connection (client) " << connection_id()
          << " upstream: lost connection with: " << remote_endpoint_
          << " due to " << ec
          << " and data to write: " << downstream_.size();
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  if (downstream_.empty()) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " upstream: last data sent: shutting down";
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  } else {
    socket_.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
  }
}

std::shared_ptr<IOBuf> Socks5Connection::DecryptData(
    std::shared_ptr<IOBuf> cipherbuf) {
  std::unique_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
  plainbuf->reserve(0, cipherbuf->length());

  DumpHex("ERead->", cipherbuf.get());
  decoder_->decrypt(cipherbuf.get(), plainbuf);
  DumpHex("PRead->", plainbuf.get());
  std::shared_ptr<IOBuf> buf{plainbuf.release()};
  return buf;
}

std::shared_ptr<IOBuf> Socks5Connection::EncryptData(
    std::shared_ptr<IOBuf> plainbuf) {
  std::unique_ptr<IOBuf> cipherbuf = IOBuf::create(plainbuf->length() + 100);
  cipherbuf->reserve(0, plainbuf->length() + 100);

  DumpHex("PWrite->", plainbuf.get());
  encoder_->encrypt(plainbuf.get(), cipherbuf);
  DumpHex("EWrite->", cipherbuf.get());
  std::shared_ptr<IOBuf> sharedBuf{cipherbuf.release()};
  return sharedBuf;
}
