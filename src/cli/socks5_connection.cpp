// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "cli/socks5_connection.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/base64.hpp"
#include "core/http_parser.hpp"
#include "core/stringprintf.hpp"
#include "core/rand_util.hpp"
#include "core/utils.hpp"

namespace {
// from spdy_session.h
// If more than this many bytes have been read or more than that many
// milliseconds have passed, return ERR_IO_PENDING from ReadLoop.
const int kYieldAfterBytesRead = 32 * 1024;
const int kYieldAfterDurationMilliseconds = 20;

std::vector<http2::adapter::Header> GenerateHeaders(std::vector<std::pair<std::string, std::string>> headers,
                                                    int status = 0) {
  std::vector<http2::adapter::Header> response_vector;
  if (status) {
    response_vector.emplace_back(
        http2::adapter::HeaderRep(std::string(":status")),
        http2::adapter::HeaderRep(std::to_string(status)));
  }
  for (const auto& header : headers) {
    // Connection (and related) headers are considered malformed and will
    // result in a client error
    if (header.first == "Connection")
      continue;
    response_vector.emplace_back(
        http2::adapter::HeaderRep(header.first),
        http2::adapter::HeaderRep(header.second));
  }

  return response_vector;
}

std::string GetProxyAuthorizationIdentity() {
  std::string result, user_pass = absl::GetFlag(FLAGS_username) + ":" +
    absl::GetFlag(FLAGS_password);
  Base64Encode(user_pass, &result);
  return result;
}

} // anonymous namespace

namespace {
bool g_nonindex_codes_initialized;
uint8_t g_nonindex_codes[17];

void InitializeNonindexCodes() {
  if (g_nonindex_codes_initialized)
    return;
  g_nonindex_codes_initialized = true;
  unsigned i = 0;
  for (const auto& symbol : spdy::HpackHuffmanCodeVector()) {
    if (symbol.id >= 0x20 && symbol.id <= 0x7f && symbol.length >= 8) {
      g_nonindex_codes[i++] = symbol.id;
      if (i >= sizeof(g_nonindex_codes))
        break;
    }
  }
  CHECK(i == sizeof(g_nonindex_codes));
}

void FillNonindexHeaderValue(uint64_t unique_bits, char* buf, int len) {
  DCHECK(g_nonindex_codes_initialized);
  int first = len < 16 ? len : 16;
  for (int i = 0; i < first; i++) {
    buf[i] = g_nonindex_codes[unique_bits & 0b1111];
    unique_bits >>= 4;
  }
  for (int i = first; i < len; i++) {
    buf[i] = g_nonindex_codes[16];
  }
}
}  // namespace

// 32K / 4k = 8
#define MAX_DOWNSTREAM_DEPS 8
#define MAX_UPSTREAM_DEPS 8

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

bool DataFrameSource::Send(absl::string_view frame_header, size_t payload_length)  {
  absl::string_view payload(
      reinterpret_cast<const char*>(chunks_.front()->data()), payload_length);
  std::string concatenated = absl::StrCat(frame_header, payload);
  const int64_t result = connection_->OnReadyToSend(concatenated);
  // Write encountered error.
  if (result < 0) {
    connection_->OnConnectionError(http2::adapter::Http2VisitorInterface::ConnectionError::kSendError);
    return false;
  }

  // Write blocked.
  if (result == 0) {
    connection_->blocked_stream_ = stream_id_;
    return false;
  }

  if (static_cast<size_t>(result) < concatenated.size()) {
    // Probably need to handle this better within this test class.
    QUICHE_LOG(DFATAL)
        << "DATA frame not fully flushed. Connection will be corrupt!";
    connection_->OnConnectionError(http2::adapter::Http2VisitorInterface::ConnectionError::kSendError);
    return false;
  }

  chunks_.front()->trimStart(payload_length);

  if (chunks_.front()->empty())
    chunks_.pop_front();

  if (chunks_.empty() && send_completion_callback_) {
    std::move(send_completion_callback_).operator()();
  }

  // Unblocked
  if (chunks_.empty()) {
    connection_->blocked_stream_ = 0;
  }

  return true;
}

Socks5Connection::Socks5Connection(asio::io_context& io_context,
                                   const asio::ip::tcp::endpoint& remote_endpoint,
                                   bool upstream_https_fallback,
                                   bool https_fallback,
                                   bool enable_upstream_tls,
                                   bool enable_tls,
                                   asio::ssl::context *upstream_ssl_ctx,
                                   asio::ssl::context *ssl_ctx)
    : Connection(io_context, remote_endpoint,
                 upstream_https_fallback, https_fallback,
                 enable_upstream_tls, enable_tls,
                 upstream_ssl_ctx, ssl_ctx),
      state_() {}

Socks5Connection::~Socks5Connection() {
  VLOG(2) << "Connection (client) " << connection_id() << " freed memory";
};

void Socks5Connection::start() {
  SetState(state_method_select);
  closed_ = false;
  upstream_writable_ = false;
  downstream_readable_ = true;
  asio::error_code ec;
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);
  ReadMethodSelect();
}

void Socks5Connection::close() {
  if (closed_) {
    return;
  }
  size_t bytes = 0;
  for (auto buf : downstream_)
    bytes += buf->length();
  VLOG(2) << "Connection (client) " << connection_id()
          << " disconnected with client at stage: "
          << Socks5Connection::state_to_str(CurrentState())
          << " and remaining: " << bytes << " bytes.";
  asio::error_code ec;
  closed_ = true;
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

void Socks5Connection::SendIfNotProcessing() {
  if (!processing_responses_) {
    processing_responses_ = true;
    adapter_->Send();
    processing_responses_ = false;
  }
}

//
// cipher_visitor_interface
//
bool Socks5Connection::on_received_data(std::shared_ptr<IOBuf> buf) {
  MSAN_CHECK_MEM_IS_INITIALIZED(buf->data(), buf->length());
  downstream_.push_back(buf);
  return true;
}

void Socks5Connection::on_protocol_error() {
  LOG(WARNING) << "Connection (client) " << connection_id()
    << " Protocol error";
  disconnected(asio::error::connection_aborted);
}

//
// http2::adapter::Http2VisitorInterface
//

int64_t Socks5Connection::OnReadyToSend(absl::string_view serialized) {
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    return kSendBlocked;
  }

  std::shared_ptr<IOBuf> buf =
    IOBuf::copyBuffer(serialized.data(), serialized.size());
  upstream_.push_back(buf);
  return serialized.size();
}

http2::adapter::Http2VisitorInterface::OnHeaderResult
Socks5Connection::OnHeaderForStream(StreamId stream_id,
                                    absl::string_view key,
                                    absl::string_view value) {
  request_map_[key] = std::string(value);
  return http2::adapter::Http2VisitorInterface::HEADER_OK;
}

bool Socks5Connection::OnEndHeadersForStream(
  http2::adapter::Http2StreamId stream_id) {
  auto padding_support = request_map_.find("padding") != request_map_.end();
  if (padding_support_ && padding_support) {
    LOG(INFO) << "Connection (client) " << connection_id() << " for "
      << remote_endpoint_ << " Padding support enabled.";
  } else {
    VLOG(2) << "Connection (client) " << connection_id() << " for "
      << remote_endpoint_ <<  " Padding support disabled.";
    padding_support_ = false;
  }
  return true;
}

bool Socks5Connection::OnEndStream(StreamId stream_id) {
  return true;
}

bool Socks5Connection::OnCloseStream(StreamId stream_id,
                                     http2::adapter::Http2ErrorCode error_code) {
  disconnected(asio::error_code());
  return true;
}

void Socks5Connection::OnConnectionError(ConnectionError /*error*/) {
  disconnected(asio::error::connection_aborted);
}

bool Socks5Connection::OnFrameHeader(StreamId stream_id,
                                     size_t /*length*/,
                                     uint8_t /*type*/,
                                     uint8_t /*flags*/) {
  if (stream_id) {
    DCHECK_EQ(stream_id, stream_id_) << "Client only support one stream";
  }
  return true;
}

bool Socks5Connection::OnBeginHeadersForStream(StreamId stream_id) {
  return true;
}

bool Socks5Connection::OnBeginDataForStream(StreamId stream_id,
                                            size_t payload_length) {
  return true;
}

bool Socks5Connection::OnDataForStream(StreamId stream_id,
                                       absl::string_view data) {
  if (padding_support_ && num_padding_recv_ < kFirstPaddings) {
    asio::error_code ec;
    // Append buf to in_middle_buf
    if (padding_in_middle_buf_) {
      padding_in_middle_buf_->reserve(0, data.size());
      memcpy(padding_in_middle_buf_->mutable_tail(), data.data(), data.size());
      padding_in_middle_buf_->append(data.size());
    } else {
      padding_in_middle_buf_ = IOBuf::copyBuffer(data.data(), data.size());
    }
    adapter_->MarkDataConsumedForStream(stream_id, data.size());

    // Deal with in_middle_buf
    while (num_padding_recv_ < kFirstPaddings) {
      auto buf = RemovePadding(padding_in_middle_buf_, ec);
      if (ec) {
        return true;
      }
      DCHECK(buf);
      downstream_.push_back(buf);
      ++num_padding_recv_;
    }
    // Deal with in_middle_buf outside paddings
    if (num_padding_recv_ >= kFirstPaddings && !padding_in_middle_buf_->empty()) {
      upstream_.push_back(std::move(padding_in_middle_buf_));
    }
    return true;
  }

  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(data.data(), data.size());
  downstream_.push_back(buf);
  adapter_->MarkDataConsumedForStream(stream_id, data.size());
  return true;
}

bool Socks5Connection::OnDataPaddingLength(StreamId stream_id,
                                           size_t padding_length) {
  adapter_->MarkDataConsumedForStream(stream_id, padding_length);
  return true;
}

bool Socks5Connection::OnGoAway(StreamId last_accepted_stream_id,
                                http2::adapter::Http2ErrorCode error_code,
                                absl::string_view opaque_data) {
  return true;
}

int Socks5Connection::OnBeforeFrameSent(uint8_t frame_type,
                                        StreamId stream_id,
                                        size_t length,
                                        uint8_t flags) {
  return 0;
}

int Socks5Connection::OnFrameSent(uint8_t frame_type,
                                  StreamId stream_id,
                                  size_t length,
                                  uint8_t flags,
                                  uint32_t error_code) {
  return 0;
}

bool Socks5Connection::OnInvalidFrame(StreamId stream_id,
                                      InvalidFrameError error) {
  return true;
}

bool Socks5Connection::OnMetadataForStream(StreamId stream_id,
                                           absl::string_view metadata) {
  return true;
}

bool Socks5Connection::OnMetadataEndForStream(StreamId stream_id) {
  return true;
}

void Socks5Connection::ReadMethodSelect() {
  scoped_refptr<Socks5Connection> self(this);

  socket_.async_read_some(asio::null_buffers(),
    [this, self](asio::error_code ec, size_t bytes_transferred) {
      std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
      if (!ec) {
        do {
          bytes_transferred = socket_.read_some(mutable_buffer(*buf), ec);
          if (ec == asio::error::interrupted) {
            continue;
          }
        } while(false);
      }
      if (ec == asio::error::try_again || ec == asio::error::would_block) {
        ReadMethodSelect();
        return;
      }
      if (ec) {
        OnDisconnect(ec);
        return;
      }
      buf->append(bytes_transferred);
      DumpHex("HANDSHAKE/METHOD_SELECT->", buf.get());

      ec = OnReadRedirHandshake(buf);
      if (ec) {
        ec = OnReadSocks5MethodSelect(buf);
      }
      if (ec) {
        ec = OnReadSocks4Handshake(buf);
      }
      if (ec) {
        ec = OnReadHttpRequest(buf);
      }
      if (ec) {
        OnDisconnect(ec);
      } else {
        ProcessReceivedData(buf, ec, buf->length());
      }
  });
}

void Socks5Connection::ReadSocks5Handshake() {
  scoped_refptr<Socks5Connection> self(this);

  socket_.async_read_some(asio::null_buffers(),
    [this, self](asio::error_code ec, size_t bytes_transferred) {
      std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
      if (!ec) {
        do {
          bytes_transferred = socket_.read_some(mutable_buffer(*buf), ec);
          if (ec == asio::error::interrupted) {
            continue;
          }
        } while(false);
      }
      if (ec == asio::error::try_again || ec == asio::error::would_block) {
        ReadSocks5Handshake();
        return;
      }
      if (ec) {
        OnDisconnect(ec);
        return;
      }
      buf->append(bytes_transferred);
      DumpHex("HANDSHAKE->", buf.get());
      ec = OnReadSocks5Handshake(buf);
      if (ec) {
        OnDisconnect(ec);
      } else {
        ProcessReceivedData(buf, ec, buf->length());
      }
  });
}

asio::error_code Socks5Connection::OnReadRedirHandshake(
    std::shared_ptr<IOBuf> buf) {
#ifdef __linux__
  VLOG(3) << "Connection (client) " << connection_id()
          << " try redir handshake";
  scoped_refptr<Socks5Connection> self(this);
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

    OnCmdConnect(endpoint);
    asio::error_code ec;

    if (buf->length()) {
      ProcessReceivedData(buf, ec, buf->length());
    } else {
      ReadStream();  // continously read
    }
    return ec;
  }
#endif
  return std::make_error_code(std::errc::network_unreachable);
}

asio::error_code Socks5Connection::OnReadSocks5MethodSelect(
    std::shared_ptr<IOBuf> buf) {
  scoped_refptr<Socks5Connection> self(this);
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

  HttpRequestParser parser;

  bool ok;
  int nparsed = parser.Parse(buf, &ok);
  if (nparsed) {
    VLOG(4) << "Connection (client) " << connection_id()
            << " http: "
            << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
  }

  if (ok) {
    buf->trimStart(nparsed);
    buf->retreat(nparsed);

    http_host_ = parser.host();
    http_port_ = parser.port();
    http_is_connect_ = parser.is_connect();

    if (!http_is_connect_) {
      std::string header;
      parser.ReforgeHttpRequest(&header);
      buf->reserve(header.size(), 0);
      buf->prepend(header.size());
      memcpy(buf->mutable_data(), header.c_str(), header.size());
      VLOG(4) << "Connection (client) " << connection_id()
              << " Host: " << http_host_ << " PORT: " << http_port_;
    } else {
      VLOG(4) << "Connection (client) " << connection_id()
              << " CONNECT: " << http_host_ << " PORT: " << http_port_;
    }

    SetState(state_http_handshake);
    VLOG(3) << "Connection (client) " << connection_id()
            << " http handshake began";
    return asio::error_code();
  }

  LOG(WARNING) << "Connection (client) " << connection_id()
               << parser.ErrorMessage() << ": "
               << std::string(reinterpret_cast<const char*>(buf->data()),
                              nparsed);
  return std::make_error_code(std::errc::bad_message);
}

void Socks5Connection::ReadStream() {
  scoped_refptr<Socks5Connection> self(this);
  downstream_read_inprogress_ = true;

  socket_.async_read_some(asio::null_buffers(),
    [this, self](asio::error_code ec, size_t bytes_transferred) {
      downstream_read_inprogress_ = false;
      if (ec) {
        ProcessReceivedData(nullptr, ec, bytes_transferred);
        return;
      }
      if (!downstream_readable_) {
        return;
      }
      std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
      if (!ec) {
        do {
          bytes_transferred = socket_.read_some(mutable_buffer(*buf), ec);
          if (ec == asio::error::interrupted) {
            continue;
          }
        } while(false);
      }
      if (ec == asio::error::try_again || ec == asio::error::would_block) {
        ReadStream();
        return;
      }
      buf->append(bytes_transferred);
      ProcessReceivedData(buf, ec, bytes_transferred);
  });
}

void Socks5Connection::WriteMethodSelect() {
  scoped_refptr<Socks5Connection> self(this);
  method_select_reply_ = socks5::method_select_response_stock_reply();
  asio::async_write(
    socket_,
    asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
    [this, self](asio::error_code ec, size_t bytes_transferred) {
      return ProcessSentData(ec, bytes_transferred);
  });
}

void Socks5Connection::WriteHandshake() {
  scoped_refptr<Socks5Connection> self(this);
  switch (CurrentState()) {
    case state_method_select:  // impossible
    case state_socks5_handshake:
      SetState(state_stream);
      asio::async_write(socket_, s5_reply_.buffers(),
        [this, self](asio::error_code ec, size_t bytes_transferred) {
          return ProcessSentData(ec, bytes_transferred);
      });
      break;
    case state_socks4_handshake:
      SetState(state_stream);
      asio::async_write(
        socket_, s4_reply_.buffers(),
        [this, self](asio::error_code ec, size_t bytes_transferred) {
          return ProcessSentData(ec, bytes_transferred);
      });
      break;
    case state_http_handshake:
      SetState(state_stream);
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
                 << static_cast<int>(CurrentState()) << std::dec;
  }
}

void Socks5Connection::WriteStream() {
  DCHECK_EQ(CurrentState(), state_stream);
  // DCHECK(!write_inprogress_);
  if (write_inprogress_) {
    return;
  }
  scoped_refptr<Socks5Connection> self(this);
  write_inprogress_ = true;
  socket_.async_write_some(asio::null_buffers(),
    [this, self](asio::error_code ec, size_t /*bytes_transferred*/) {
      write_inprogress_ = false;
      if (ec) {
        ProcessSentData(ec, 0);
        return;
      }
      WriteStreamInPipe();
  });
}

void Socks5Connection::WriteStreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0;
  uint64_t next_ticks = GetMonotonicTime() +
    kYieldAfterDurationMilliseconds * 1000 * 1000;

  /* recursively send the remainings */
  while (!closed_) {
    if (GetMonotonicTime() > next_ticks) {
      break;
    }
    if (bytes_transferred > kYieldAfterBytesRead) {
      break;
    }

    bool eof = false;
    auto buf = GetNextDownstreamBuf(ec);
    size_t read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      eof = true;
    } else if (ec) {
      /* safe to return, channel will handle this error */
      ec = asio::error_code();
      break;
    }
    if (!read) {
      break;
    }
    size_t written;
    do {
      ec = asio::error_code();
      written = socket_.write_some(const_buffer(*buf), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->trimStart(written);
    bytes_transferred += written;
    // continue to resume
    if (buf->empty()) {
      downstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      break;
    }
    if (ec) {
      break;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }
  ProcessSentData(ec, bytes_transferred);
}

std::shared_ptr<IOBuf> Socks5Connection::GetNextDownstreamBuf(asio::error_code &ec) {
  if (!downstream_.empty()) {
    ec = asio::error_code();
    return downstream_.front();
  }
  if (!upstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_DEBUF_SIZE);
  size_t read;
  do {
    ec = asio::error_code();
    read = channel_->read_some(buf, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (read) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " upstream: received reply (pipe): " << read << " bytes.";
  } else {
    return nullptr;
  }
  if (adapter_) {
    absl::string_view remaining_buffer(
        reinterpret_cast<const char*>(buf->data()), buf->length());
    while (!remaining_buffer.empty()) {
      int result = adapter_->ProcessBytes(remaining_buffer);
      if (result < 0) {
        ec = asio::error::connection_refused;
        disconnected(asio::error::connection_refused);
        return nullptr;
      }
      remaining_buffer = remaining_buffer.substr(result);
    }
    // Sent Control Streams
    SendIfNotProcessing();
    OnUpstreamWriteFlush();
  } else if (upstream_https_fallback_) {
    downstream_.push_back(buf);
  } else {
    decoder_->process_bytes(buf);
  }
  if (downstream_.empty()) {
    ec = asio::error::try_again;
    return nullptr;
  }
  return downstream_.front();
}

void Socks5Connection::WriteUpstreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0U;
  uint64_t next_ticks = GetMonotonicTime() +
    kYieldAfterDurationMilliseconds * 1000 * 1000;

  /* recursively send the remainings */
  while (!channel_->eof()) {
    if (bytes_transferred > kYieldAfterBytesRead) {
      break;
    }
    if (GetMonotonicTime() > next_ticks) {
      break;
    }
    bool eof = false;
    size_t read;
    std::shared_ptr<IOBuf> buf = GetNextUpstreamBuf(ec);
    read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      eof = true;
    } else if (ec) {
      /* safe to return, socket will handle this error later */
      return;
    }
    if (!read) {
      break;
    }
    ec = asio::error_code();
    size_t written;
    do {
      written = channel_->write_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->trimStart(written);
    bytes_transferred += written;
    VLOG(3) << "Connection (client) " << connection_id()
            << " upstream: sent request (pipe): " << written << " bytes"
            << " ec: " << ec << " and data to write: "
            << buf->length();
    // continue to resume
    if (buf->empty()) {
      upstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }
}

std::shared_ptr<IOBuf> Socks5Connection::GetNextUpstreamBuf(asio::error_code &ec) {
  if (!upstream_.empty()) {
    ec = asio::error_code();
    return upstream_.front();
  }
  if (!downstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  size_t bytes_transferred = 0U;

repeat_fetch:
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  size_t read;
  do {
    read = socket_.read_some(mutable_buffer(*buf), ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    /* safe to return, socket will handle this error later */
    ProcessReceivedData(nullptr, ec, read);
    return nullptr;
  }
  if (read) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " received data (pipe): " << read << " bytes.";
  } else {
    goto out;
  }
  rbytes_transferred_ += read;
  total_rx_bytes += read;
  bytes_transferred += read;

  if (adapter_) {
    if (padding_support_ && num_padding_send_ < kFirstPaddings) {
      ++num_padding_send_;
      AddPadding(buf);
    }
    data_frame_->AddChunk(buf);
    if (bytes_transferred <= kYieldAfterBytesRead) {
      goto repeat_fetch;
    }
  } else if (upstream_https_fallback_) {
    upstream_.push_back(buf);
  } else {
    upstream_.push_back(EncryptData(buf));
  }

out:
  if (adapter_ && bytes_transferred) {
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter_->ResumeStream(stream_id_);
    SendIfNotProcessing();
  }
  if (upstream_.empty()) {
    ec = asio::error::try_again;
    return nullptr;
  }
  return upstream_.front();
}

asio::error_code Socks5Connection::PerformCmdOpsV5(
    const socks5::request* request,
    socks5::reply* reply) {
  asio::error_code ec;

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

      if (request->address_type() == socks5::domain) {
        OnCmdConnect(request->domain_name(), request->port());
      } else {
        OnCmdConnect(request->endpoint());
      }
    } break;
    case socks5::cmd_bind:
    case socks5::cmd_udp_associate:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks5::reply::request_failed_cmd_not_supported;
      ec = asio::error::invalid_argument;
      break;
  }
  return ec;
}

asio::error_code Socks5Connection::PerformCmdOpsV4(
    const socks4::request* request,
    socks4::reply* reply) {
  asio::error_code ec;

  switch (request->command()) {
    case socks4::cmd_connect: {
      asio::ip::tcp::endpoint endpoint;

      if (request->is_socks4a()) {
        // TBD
        LOG(WARNING) << "Connection (client) " << connection_id()
                     << " not supported protocol socks4a";
        ec = asio::error::invalid_argument;
      }
      endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);

      if (ec) {
        reply->mutable_status() = socks4::reply::request_failed;
      } else {
        reply->set_endpoint(endpoint);
        // reply->set_loopback();
        reply->mutable_status() = socks4::reply::request_granted;
      }

      if (request->is_socks4a()) {
        OnCmdConnect(request->domain_name(), request->port());
      } else {
        OnCmdConnect(request->endpoint());
      }
    } break;
    case socks4::cmd_bind:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks4::reply::request_failed;
      ec = asio::error::invalid_argument;
      break;
  }
  return ec;
}

asio::error_code Socks5Connection::PerformCmdOpsHttp() {
  OnCmdConnect(http_host_, http_port_);

  return asio::error_code();
}

void Socks5Connection::ProcessReceivedData(
    std::shared_ptr<IOBuf> buf,
    asio::error_code ec,
    size_t bytes_transferred) {

  VLOG(3) << "Connection (client) " << connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " ec: " << ec;

  rbytes_transferred_ += bytes_transferred;
  total_rx_bytes += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (CurrentState()) {
      case state_method_select:
        WriteMethodSelect();
        break;
      case state_socks5_handshake:
        ec = PerformCmdOpsV5(&s5_request_, &s5_reply_);
        WriteHandshake();
        VLOG(3) << "Connection (client) " << connection_id()
                << " socks5 handshake finished";
        if (CurrentState() == state_stream) {
          goto handle_stream;
        }
        break;
      case state_socks4_handshake:
        ec = PerformCmdOpsV4(&s4_request_, &s4_reply_);
        WriteHandshake();
        VLOG(3) << "Connection (client) " << connection_id()
                << " socks4 handshake finished";
        if (CurrentState() == state_stream) {
          goto handle_stream;
        }
        break;
      case state_http_handshake:
        ec = PerformCmdOpsHttp();
        WriteHandshake();
        VLOG(3) << "Connection (client) " << connection_id()
                << " http handshake finished";
        if (CurrentState() == state_stream) {
          goto handle_stream;
        }
        break;
      case state_stream:
      handle_stream:
        if (buf->length()) {
          OnStreamRead(buf);
        }
        ReadStream();  // continously read
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    };
  }
#if 1
  // Silence Read EOF error triggered by upstream disconnection
  if (ec == asio::error::eof && channel_ && channel_->eof()) {
    return;
  }
#endif
  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
};

void Socks5Connection::ProcessSentData(asio::error_code ec,
                                       size_t bytes_transferred) {

  VLOG(3) << "Connection (client) " << connection_id()
          << " sent data: " << bytes_transferred << " bytes"
          << " ec: " << ec << " and data to write: " << downstream_.size();

  wbytes_transferred_ += bytes_transferred;
  total_tx_bytes += bytes_transferred;

  if (!ec) {
    switch (CurrentState()) {
      case state_method_select:
        ReadSocks5Handshake();  // read next state info
        break;
      case state_socks5_handshake:
      case state_socks4_handshake:
      case state_http_handshake:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      case state_stream:
        OnStreamWrite();
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    }
  }

  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
};

void Socks5Connection::OnCmdConnect(const asio::ip::tcp::endpoint& endpoint) {
  ss_request_ = std::make_unique<ss::request>(endpoint);
  OnConnect();
}

void Socks5Connection::OnCmdConnect(const std::string& domain_name, uint16_t port) {
  ss_request_ = std::make_unique<ss::request>(domain_name, port);
  OnConnect();
}

void Socks5Connection::OnConnect() {
  LOG(INFO) << "Connection (client) " << connection_id()
            << " to " << remote_domain();
  // create lazy
  channel_ = std::make_unique<stream>(*io_context_, remote_endpoint_,
                                      this, upstream_https_fallback_,
                                      enable_upstream_tls_, upstream_ssl_ctx_);
  channel_->connect();
}

void Socks5Connection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  // queue limit to downstream read
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    VLOG(2) << "Connection (client) " << connection_id()
            << " disabling reading";
    DisableStreamRead();
  }

  if (!channel_->connected()) {
    pending_data_ = buf;
    DisableStreamRead();
    return;
  }

  // SendContents
  if (adapter_) {
    if (padding_support_ && num_padding_send_ < kFirstPaddings) {
      ++num_padding_send_;
      AddPadding(buf);
    }
    data_frame_->AddChunk(buf);
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter()->ResumeStream(stream_id_);
    SendIfNotProcessing();
  } else if (upstream_https_fallback_) {
    upstream_.push_back(buf);
  } else {
    upstream_.push_back(EncryptData(buf));
  }
  OnUpstreamWriteFlush();
}

void Socks5Connection::OnStreamWrite() {
  OnDownstreamWriteFlush();

  /* shutdown the socket if upstream is eof and all remaining data sent */
  if (channel_->eof() && downstream_.empty()) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " last data sent: shutting down";
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    return;
  }

  /* disable queue limit to re-enable upstream read */
  if (channel_->connected() && downstream_.size() < MAX_DOWNSTREAM_DEPS && !upstream_readable_) {
    VLOG(2) << "Connection (client) " << connection_id()
            << " re-enabling reading from upstream";
    upstream_readable_ = true;
    scoped_refptr<Socks5Connection> self(this);
    channel_->enable_read([self]() {}, SOCKET_DEBUF_SIZE);
  }
}

void Socks5Connection::EnableStreamRead() {
  if (!downstream_readable_) {
    downstream_readable_ = true;
    if (!downstream_read_inprogress_) {
      ReadStream();
    }
  }
}

void Socks5Connection::DisableStreamRead() {
  downstream_readable_ = false;
}

void Socks5Connection::OnDisconnect(asio::error_code ec) {
  size_t bytes = 0;
  for (auto buf : downstream_)
    bytes += buf->length();
#ifdef WIN32
  if (ec.value() == WSAESHUTDOWN) {
    ec = asio::error_code();
  }
#else
  if (ec.value() == asio::error::operation_aborted) {
    ec = asio::error_code();
  }
#endif
  LOG(INFO) << "Connection (client) " << connection_id()
            << " closed: " << ec << " remaining: " << bytes << " bytes";
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
  if (!downstream_.empty()) {
    if (CurrentState() == state_error) {
      VLOG(2) << "Connection (client) " << connection_id()
              << " failed to sending " << buf->length() << " bytes.";
      return;
    }
    WriteStream();
  }
}

void Socks5Connection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void Socks5Connection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " upstream: ready to send request: " << buf->length() << " bytes.";
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    scoped_refptr<Socks5Connection> self(this);
    channel_->start_write(upstream_.front(), [self](){});
  }
}

void Socks5Connection::connected() {
  VLOG(3) << "Connection (client) " << connection_id()
          << " remote: established upstream connection with: "
          << remote_domain();

  bool http2 = absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2;
  http2 |= (absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2_TLS);
  if (http2 && channel_->https_fallback()) {
    http2 = false;
    upstream_https_fallback_ = true;
  }

  // Create adapters
  if (http2) {
    http2::adapter::OgHttp2Adapter::Options options;
    options.perspective = http2::adapter::Perspective::kClient;
    adapter_ = http2::adapter::OgHttp2Adapter::Create(*this, options);
    padding_support_ = absl::GetFlag(FLAGS_padding_support);
  } else if (upstream_https_fallback_) {
    // nothing to create
    // TODO should we support it?
    // padding_support_ = absl::GetFlag(FLAGS_padding_support);
  } else {
    encoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password),
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
      this, true);
    decoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password),
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
      this);
  }

  // Send Upstream Header
  if (adapter_) {
    std::string host;
    int port;
    if (ss_request_->address_type() == ss::domain) {
      host = ss_request_->domain_name();
      port = ss_request_->port();
    } else {
      auto endpoint = ss_request_->endpoint();
      host = endpoint.address().to_string();
      port = endpoint.port();
    }

    std::unique_ptr<DataFrameSource> data_frame =
      std::make_unique<DataFrameSource>(this);
    data_frame_ = data_frame.get();
    std::vector<std::pair<std::string, std::string>> headers;
    headers.push_back({":method", "CONNECT"});
    //    authority   = [ userinfo "@" ] host [ ":" port ]
    headers.push_back({":authority", host + ":" + std::to_string(port)});
    headers.push_back({"host", host + ":" + std::to_string(port)});
    headers.push_back({"proxy-authorization", "basic " + GetProxyAuthorizationIdentity()});
    // Send "Padding" header
    // originated from naive_proxy_delegate.go;func ServeHTTP
    if (padding_support_) {
      // Sends client-side padding header regardless of server support
      std::string padding(RandInt(16, 32), '~');
      InitializeNonindexCodes();
      FillNonindexHeaderValue(RandUint64(), &padding[0], padding.size());
      headers.emplace_back("padding", padding);
    }
    stream_id_ = adapter_->SubmitRequest(GenerateHeaders(headers),
                                         std::move(data_frame), nullptr);
    data_frame_->set_stream_id(stream_id_);
    SendIfNotProcessing();
  } else if (upstream_https_fallback_) {
    std::string host;
    int port;
    if (ss_request_->address_type() == ss::domain) {
      host = ss_request_->domain_name();
      port = ss_request_->port();
    } else {
      auto endpoint = ss_request_->endpoint();
      host = endpoint.address().to_string();
      port = endpoint.port();
    }
    std::string hdr = StringPrintf(
        "CONNECT %s:%d HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Proxy-Connection: Keep-Alive\r\n"
        "\r\n", host.c_str(), port, host.c_str(), port);
    std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(hdr.data(), hdr.size());
    // write variable address directly as https header
    OnUpstreamWrite(buf);
  } else {
    ByteRange req(ss_request_->data(), ss_request_->length());
    std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
    // write variable address directly as ss header
    OnUpstreamWrite(EncryptData(buf));
  }

  // Re-process the read data in pending
  if (auto pending_data = std::move(pending_data_)) {
    OnStreamRead(pending_data);
    EnableStreamRead();
  }

  scoped_refptr<Socks5Connection> self(this);
  upstream_readable_ = true;
  upstream_writable_ = true;
  channel_->start_read([self]() {}, SOCKET_DEBUF_SIZE);
  OnUpstreamWriteFlush();
}

void Socks5Connection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " upstream: received reply: " << buf->length() << " bytes.";

  // queue limit to upstream read
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    VLOG(2) << "Connection (client) " << connection_id()
            << " disabling reading from upstream";
    upstream_readable_ = false;
    channel_->disable_read();
  }

  if (adapter_) {
    absl::string_view remaining_buffer(
        reinterpret_cast<const char*>(buf->data()), buf->length());
    while (!remaining_buffer.empty()) {
      int result = adapter_->ProcessBytes(remaining_buffer);
      if (result < 0) {
        disconnected(asio::error::connection_refused);
        return;
      }
      remaining_buffer = remaining_buffer.substr(result);
    }
    // Sent Control Streams
    SendIfNotProcessing();
    OnUpstreamWriteFlush();
  } else if (upstream_https_fallback_) {
    if (upstream_handshake_) {
      upstream_handshake_= false;
      HttpResponseParser parser;

      bool ok;
      int nparsed = parser.Parse(buf, &ok);

      if (nparsed) {
        VLOG(4) << "Connection (client) " << connection_id()
                << " http: "
                << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
      }
      if (ok && parser.status_code() == 200) {
        buf->trimStart(nparsed);
        buf->retreat(nparsed);
      } else {
        if (!ok) {
          LOG(WARNING) << "Connection (client) " << connection_id()
                       << " upstream server unhandled: "
                       << parser.ErrorMessage() << ": "
                       << std::string(reinterpret_cast<const char*>(buf->data()),
                                      nparsed);
        } else {
          LOG(WARNING) << "Connection (client) " << connection_id()
                       << " upstream server returns: " << parser.status_code();
        }
        disconnected(asio::error::connection_refused);
        return;
      }
    }
    if (!buf->empty()) {
      downstream_.push_back(buf);
    }
  } else {
    decoder_->process_bytes(buf);
  }
  OnDownstreamWriteFlush();
}

void Socks5Connection::sent(std::shared_ptr<IOBuf> buf, size_t bytes_transferred) {
  VLOG(3) << "Connection (client) " << connection_id()
          << " upstream: sent request: " << bytes_transferred << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  upstream_writable_ = true;

  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();

  if (blocked_stream_) {
    adapter_->ResumeStream(blocked_stream_);
    SendIfNotProcessing();
    OnUpstreamWriteFlush();
  }
  if (upstream_.size() < MAX_UPSTREAM_DEPS && !downstream_readable_) {
    VLOG(2) << "Connection (client) " << connection_id()
            << " re-enabling reading";
    EnableStreamRead();
  }
}

void Socks5Connection::disconnected(asio::error_code ec) {
  VLOG(2) << "Connection (client) " << connection_id()
          << " upstream: lost connection with: " << remote_domain()
          << " due to " << ec
          << " and data to write: " << downstream_.size();
  upstream_readable_ = false;
  upstream_writable_ = false;
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

std::shared_ptr<IOBuf> Socks5Connection::EncryptData(
    std::shared_ptr<IOBuf> plainbuf) {
  std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(plainbuf->length() + 100);

  encoder_->encrypt(plainbuf.get(), &cipherbuf);
  MSAN_CHECK_MEM_IS_INITIALIZED(cipherbuf->data(), cipherbuf->length());

  return cipherbuf;
}
