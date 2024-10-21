// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "cli/cli_connection.hpp"

#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <base/rand_util.h>
#include <base/strings/string_util.h>

#include "config/config.hpp"
#include "core/utils.hpp"
#include "net/asio.hpp"
#include "net/base64.hpp"
#include "net/http_parser.hpp"
#include "net/padding.hpp"
#include "net/socks4_request_parser.hpp"
#include "net/socks5_request_parser.hpp"

#include <build/build_config.h>

#ifdef HAVE_QUICHE
#include <quiche/http2/hpack/hpack_constants.h>
#endif

static_assert(TLSEXT_MAXLEN_host_name == uint8_t(~0));

using std::string_literals::operator""s;
using std::string_view_literals::operator""sv;
using namespace net;

#ifdef HAVE_QUICHE
static std::vector<http2::adapter::Header> GenerateHeaders(std::vector<std::pair<std::string, std::string>> headers,
                                                           int status = 0) {
  std::vector<http2::adapter::Header> response_vector;
  if (status) {
    response_vector.emplace_back(http2::adapter::HeaderRep(":status"sv),
                                 http2::adapter::HeaderRep(std::to_string(status)));
  }
  for (const auto& header : headers) {
    // Connection (and related) headers are considered malformed and will
    // result in a client error
    if (header.first == "Connection"sv)
      continue;
    response_vector.emplace_back(http2::adapter::HeaderRep(header.first), http2::adapter::HeaderRep(header.second));
  }

  return response_vector;
}

static std::string GetProxyAuthorizationIdentity() {
  auto user_pass = absl::StrCat(absl::GetFlag(FLAGS_username), ":", absl::GetFlag(FLAGS_password));
  return Base64Encode(as_bytes(make_span(user_pass)));
}

static bool g_nonindex_codes_initialized;
static uint8_t g_nonindex_codes[17];

static void InitializeNonindexCodes() {
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

static void FillNonindexHeaderValue(uint64_t unique_bits, char* buf, int len) {
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
#endif

namespace net::cli {

constexpr const std::string_view CliConnection::http_connect_reply_ = "HTTP/1.1 200 Connection established\r\n\r\n";

#if BUILDFLAG(IS_MAC)
#include <xnu_private/net_pfvar.h>
#endif
#ifdef __linux__
#include <linux/netfilter_ipv4.h>
#endif

#ifdef __linux__
static bool IsIPv4MappedIPv6(const asio::ip::tcp::endpoint& address) {
  return address.address().is_v6() && address.address().to_v6().is_v4_mapped();
}
#endif

#ifdef HAVE_QUICHE
bool DataFrameSource::Send(absl::string_view frame_header, size_t payload_length) {
  std::string concatenated;
  if (payload_length) {
    DCHECK(!chunks_.empty());
    absl::string_view payload(reinterpret_cast<const char*>(chunks_.front()->data()), payload_length);
    concatenated = absl::StrCat(frame_header, payload);
  } else {
    concatenated = std::string{frame_header};
  }

  const int64_t result = connection_->OnReadyToSend(concatenated);
  DCHECK_EQ(static_cast<size_t>(result), concatenated.size());

  if (!payload_length) {
    return true;
  }

  chunks_.front()->trimStart(payload_length);

  if (chunks_.front()->empty()) {
    chunks_.pop_front();
  }

  if (chunks_.empty() && send_completion_callback_) {
    std::move(send_completion_callback_).operator()();
  }

  // Unblocked
  if (chunks_.empty()) {
    connection_->blocked_stream_ = 0;
  }

  return true;
}
#endif

CliConnection::CliConnection(asio::io_context& io_context,
                             std::string_view remote_host_ips,
                             std::string_view remote_host_sni,
                             uint16_t remote_port,
                             bool upstream_https_fallback,
                             bool https_fallback,
                             bool enable_upstream_tls,
                             bool enable_tls,
                             SSL_CTX* upstream_ssl_ctx,
                             SSL_CTX* ssl_ctx)
    : Connection(io_context,
                 remote_host_ips,
                 remote_host_sni,
                 remote_port,
                 upstream_https_fallback,
                 https_fallback,
                 enable_upstream_tls,
                 enable_tls,
                 upstream_ssl_ctx,
                 ssl_ctx),
      state_(),
      resolver_(io_context) {}

CliConnection::~CliConnection() {
  VLOG(1) << "Connection (client) " << connection_id() << " freed memory";
}

void CliConnection::start() {
  SetState(state_method_select);
  closed_ = false;
  upstream_writable_ = false;
  downstream_readable_ = true;

  ReadMethodSelect();
}

void CliConnection::close() {
  if (closed_) {
    return;
  }
  VLOG(1) << "Connection (client) " << connection_id()
          << " disconnected with client at stage: " << CliConnection::state_to_str(CurrentState());
  asio::error_code ec;
  closed_ = true;
  resolver_.Reset();
  downlink_->close(ec);
  if (ec) {
    VLOG(1) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  on_disconnect();
}

#ifdef HAVE_QUICHE
void CliConnection::SendIfNotProcessing() {
  DCHECK(!http2_in_recv_callback_);
  if (!processing_responses_) {
    processing_responses_ = true;
    while (adapter_->want_write() && adapter_->Send() == 0) {
    }
    processing_responses_ = false;
  }
}
#endif

//
// cipher_visitor_interface
//
bool CliConnection::on_received_data(std::shared_ptr<IOBuf> buf) {
  if (buf->empty()) {
    return false;
  }
  downstream_.push_back(buf);
  return true;
}

void CliConnection::on_protocol_error() {
  LOG(WARNING) << "Connection (client) " << connection_id() << " Protocol error";
  disconnected(asio::error::connection_aborted);
}

#ifdef HAVE_QUICHE
//
// http2::adapter::Http2VisitorInterface
//

int64_t CliConnection::OnReadyToSend(absl::string_view serialized) {
  upstream_.push_back(serialized.data(), serialized.size());
  return serialized.size();
}

http2::adapter::Http2VisitorInterface::OnHeaderResult CliConnection::OnHeaderForStream(StreamId stream_id,
                                                                                       absl::string_view key,
                                                                                       absl::string_view value) {
  request_map_[key] = std::string{value};
  return http2::adapter::Http2VisitorInterface::HEADER_OK;
}

bool CliConnection::OnEndHeadersForStream(http2::adapter::Http2StreamId stream_id) {
  bool padding_support = request_map_.find("padding"s) != request_map_.end();
  padding_support_ &= padding_support;
  std::string_view server_field = "(unknown)"sv;
  auto it = request_map_.find("server"s);
  if (it != request_map_.end()) {
    server_field = it->second;
  }
  LOG(INFO) << "Connection (client) " << connection_id() << " for " << remote_domain() << " Padding support "
            << (padding_support_ ? "enabled" : "disabled") << " Backed by " << server_field << ".";
  return true;
}

bool CliConnection::OnEndStream(StreamId stream_id) {
  if (stream_id == stream_id_) {
    data_frame_ = nullptr;
    stream_id_ = 0;
    adapter_->SubmitGoAway(0, http2::adapter::Http2ErrorCode::HTTP2_NO_ERROR, ""sv);
    DCHECK(adapter_->want_write());
  }
  return true;
}

bool CliConnection::OnCloseStream(StreamId stream_id, http2::adapter::Http2ErrorCode error_code) {
  if (stream_id == 0 || stream_id == stream_id_) {
#ifdef HAVE_NGHTTP2
    if (stream_id_) {
      adapter_->RemoveStream(stream_id_);
    }
#endif
    data_frame_ = nullptr;
    stream_id_ = 0;
  }
  return true;
}

void CliConnection::OnConnectionError(ConnectionError error) {
  LOG(INFO) << "Connection (client) " << connection_id() << " http2 connection error: " << (int)error;
  data_frame_ = nullptr;
  stream_id_ = 0;
  disconnected(asio::error::invalid_argument);
}

bool CliConnection::OnFrameHeader(StreamId stream_id, size_t /*length*/, uint8_t /*type*/, uint8_t /*flags*/) {
  return true;
}

bool CliConnection::OnBeginHeadersForStream(StreamId stream_id) {
  if (stream_id) {
    DCHECK_EQ(stream_id, stream_id_) << "Client only support one stream";
  }
  return true;
}

bool CliConnection::OnBeginDataForStream(StreamId stream_id, size_t payload_length) {
  return true;
}

bool CliConnection::OnDataForStream(StreamId stream_id, absl::string_view data) {
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
      downstream_.push_back(std::move(padding_in_middle_buf_));
    }
    return true;
  }

  downstream_.push_back(data.data(), data.size());
  adapter_->MarkDataConsumedForStream(stream_id, data.size());
  return true;
}

bool CliConnection::OnDataPaddingLength(StreamId stream_id, size_t padding_length) {
  adapter_->MarkDataConsumedForStream(stream_id, padding_length);
  return true;
}

void CliConnection::OnRstStream(StreamId stream_id, http2::adapter::Http2ErrorCode error_code) {
  disconnected(asio::error::connection_reset);
}

bool CliConnection::OnGoAway(StreamId last_accepted_stream_id,
                             http2::adapter::Http2ErrorCode error_code,
                             absl::string_view opaque_data) {
  disconnected(asio::error::eof);
  return true;
}

int CliConnection::OnBeforeFrameSent(uint8_t frame_type, StreamId stream_id, size_t length, uint8_t flags) {
  return 0;
}

int CliConnection::OnFrameSent(uint8_t frame_type,
                               StreamId stream_id,
                               size_t length,
                               uint8_t flags,
                               uint32_t error_code) {
  return 0;
}

bool CliConnection::OnInvalidFrame(StreamId stream_id, InvalidFrameError error) {
  return true;
}

bool CliConnection::OnMetadataForStream(StreamId stream_id, absl::string_view metadata) {
  return true;
}

bool CliConnection::OnMetadataEndForStream(StreamId stream_id) {
  return true;
}

#endif

void CliConnection::ReadMethodSelect() {
  scoped_refptr<CliConnection> self(this);

  downlink_->async_read_some([this, self](asio::error_code ec) {
    if (closed_) {
      return;
    }
    if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      ProcessReceivedData(nullptr, ec, 0);
      return;
    }
    std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
    size_t bytes_transferred;
    do {
      bytes_transferred = downlink_->read_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while (false);
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
    if (ec == asio::error::operation_not_supported) {
      ec = asio::error::invalid_argument;
    }
    if (ec == asio::error::invalid_argument) {
      ec = OnReadSocks5MethodSelect(buf);
    }
    if (ec == asio::error::invalid_argument) {
      ec = OnReadSocks4Handshake(buf);
    }
    if (ec == asio::error::invalid_argument) {
      ec = OnReadHttpRequest(buf);
    }
    if (ec) {
      OnDisconnect(ec);
    } else {
      ProcessReceivedData(buf, ec, buf->length());
    }
  });
}

void CliConnection::ReadSocks5Handshake() {
  scoped_refptr<CliConnection> self(this);

  downlink_->async_read_some([this, self](asio::error_code ec) {
    if (closed_) {
      return;
    }
    if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      ProcessReceivedData(nullptr, ec, 0);
      return;
    }
    std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
    size_t bytes_transferred;
    do {
      bytes_transferred = downlink_->read_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while (false);
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

asio::error_code CliConnection::OnReadRedirHandshake(std::shared_ptr<IOBuf> buf) {
#if BUILDFLAG(IS_MAC)
  if (!absl::GetFlag(FLAGS_redir_mode)) {
    return asio::error::operation_not_supported;
  }
  VLOG(2) << "Connection (client) " << connection_id() << " try redir handshake";
  scoped_refptr<CliConnection> self(this);
  const bool ipv4_compatible = peer_endpoint_.address().is_v4();

  int pf_fd = open("/dev/pf", 0, O_RDONLY);
  if (pf_fd < 0) {
    PLOG(WARNING) << "pf not connected";
    return asio::error::operation_not_supported;
  }
  struct pfioc_natlook pnl;
  pnl.direction = PF_OUT;
  pnl.proto = IPPROTO_TCP;

  union sockaddr_storage_union {
    struct sockaddr_storage ss;
    struct sockaddr_in s4;
    struct sockaddr_in6 s6;
  };
  sockaddr_storage_union peer_ss = {}, ss = {};
  memcpy(&peer_ss, peer_endpoint_.data(), peer_endpoint_.size());
  memcpy(&ss, endpoint_.data(), endpoint_.size());

  if (ipv4_compatible) {
    pnl.af = AF_INET;
    memcpy(&pnl.saddr.v4addr, &peer_ss.s4.sin_addr.s_addr, sizeof pnl.saddr.v4addr);
    memcpy(&pnl.daddr.v4addr, &ss.s4.sin_addr.s_addr, sizeof pnl.daddr.v4addr);
    pnl.sxport.port = peer_ss.s4.sin_port;
    pnl.dxport.port = ss.s4.sin_port;
  } else {
    pnl.af = AF_INET6;
    memcpy(&pnl.saddr.v6addr, &peer_ss.s6.sin6_addr.s6_addr, sizeof pnl.saddr.v6addr);
    memcpy(&pnl.daddr.v6addr, &ss.s6.sin6_addr.s6_addr, sizeof pnl.daddr.v6addr);
    pnl.sxport.port = peer_ss.s6.sin6_port;
    pnl.dxport.port = ss.s6.sin6_port;
  }
  if (ioctl(pf_fd, DIOCNATLOOK, &pnl) < 0) {
    PLOG(WARNING) << "DIOCNATLOOK failed on pf";
    ::close(pf_fd);
    return asio::error::operation_not_supported;
  }
  ::close(pf_fd);

  asio::ip::tcp::endpoint endpoint;
  if (pnl.af == AF_INET) {
    endpoint.resize(sizeof(struct sockaddr_in));
    auto in = reinterpret_cast<struct sockaddr_in*>(endpoint.data());
    memset(in, 0, sizeof(struct sockaddr_in));

    in->sin_family = AF_INET;
    memcpy(&in->sin_addr.s_addr, &pnl.rdaddr.v4addr, sizeof pnl.rdaddr.v4addr);
    in->sin_port = pnl.rdxport.port;
  } else {
    endpoint.resize(sizeof(struct sockaddr_in6));
    auto in6 = reinterpret_cast<struct sockaddr_in6*>(endpoint.data());
    memset(in6, 0, sizeof(struct sockaddr_in6));

    in6->sin6_family = AF_INET6;
    memcpy(&in6->sin6_addr.s6_addr, &pnl.rdaddr.v6addr, sizeof pnl.rdaddr.v6addr);
    in6->sin6_port = pnl.rdxport.port;
  }

  if (endpoint.address().is_unspecified() || endpoint.port() == 0u) {
    LOG(WARNING) << "Connection (client) " << connection_id() << " redir: requested empty host or invalid port";
    return asio::error::invalid_argument;
  }

  VLOG(2) << "Connection (client) " << connection_id() << " redir stream from " << endpoint_ << " to " << endpoint;
  OnCmdConnect(endpoint);

  asio::error_code ec;
  if (!buf->empty()) {
    ProcessReceivedData(buf, ec, buf->length());
  } else {
    WriteUpstreamInPipe();
    OnUpstreamWriteFlush();
  }
  return ec;
#elif defined(__linux__)
  if (!absl::GetFlag(FLAGS_redir_mode)) {
    return asio::error::operation_not_supported;
  }
  VLOG(2) << "Connection (client) " << connection_id() << " try redir handshake";
  scoped_refptr<CliConnection> self(this);
  struct sockaddr_storage ss = {};
  socklen_t ss_len = sizeof(struct sockaddr_in6);
  asio::ip::tcp::endpoint endpoint;
  int ret;
  if (peer_endpoint_.address().is_v4() || IsIPv4MappedIPv6(peer_endpoint_))
    ret = getsockopt(downlink_->socket_.native_handle(), SOL_IP, SO_ORIGINAL_DST, &ss, &ss_len);
  else
    ret = getsockopt(downlink_->socket_.native_handle(), SOL_IPV6, SO_ORIGINAL_DST, &ss, &ss_len);
  if (ret == 0) {
    endpoint.resize(ss_len);
    memcpy(endpoint.data(), &ss, ss_len);
  }

  if (endpoint.address().is_unspecified() || endpoint.port() == 0u) {
    LOG(WARNING) << "Connection (client) " << connection_id() << " redir: requested empty host or invalid port";
    return asio::error::invalid_argument;
  }

  if (ret == 0 && endpoint != endpoint_) {
    // no handshake required to be written
    SetState(state_stream);

    // FindNameByAddr routine
    char hostname[NI_MAXHOST];
    char service[NI_MAXSERV];
    uint16_t port = endpoint.port();
    int ret = getnameinfo(reinterpret_cast<const struct sockaddr*>(endpoint.data()), endpoint.size(), hostname,
                          sizeof(hostname), service, sizeof(service), NI_NAMEREQD);
    if (ret == 0 && strlen(hostname) != 0 && strlen(hostname) <= TLSEXT_MAXLEN_host_name) {
      VLOG(2) << "Connection (client) " << connection_id() << " redir stream from " << hostname << ":" << port << " to "
              << endpoint;
      OnCmdConnect(hostname, port);
    } else {
      if (ret) {
        VLOG(3) << "Connection (client) " << connection_id() << " redir getnameinfo failure: " << gai_strerror(ret);
      } else if (strlen(hostname) > TLSEXT_MAXLEN_host_name) {
        LOG(WARNING) << "Connection (client) " << connection_id() << " redir too long domain name: " << hostname;
      } else {
        VLOG(3) << "Connection (client) " << connection_id() << " redir getnameinfo failure: truncated host name";
      }
      VLOG(2) << "Connection (client) " << connection_id() << " redir stream from " << endpoint_ << " to " << endpoint;
      OnCmdConnect(endpoint);
    }

    asio::error_code ec;
    if (!buf->empty()) {
      ProcessReceivedData(buf, ec, buf->length());
    } else {
      WriteUpstreamInPipe();
      OnUpstreamWriteFlush();
    }
    return ec;
  }
#endif
  return asio::error::operation_not_supported;
}

asio::error_code CliConnection::OnReadSocks5MethodSelect(std::shared_ptr<IOBuf> buf) {
  scoped_refptr<CliConnection> self(this);
  socks5::method_select_request_parser parser;
  socks5::method_select_request request;
  socks5::method_select_request_parser::result_type result;
  std::tie(result, std::ignore) = parser.parse(request, buf->data(), buf->data() + buf->length());

  if (result == socks5::method_select_request_parser::good) {
    DCHECK_LE(request.length(), buf->length());
    buf->trimStart(request.length());
    buf->retreat(request.length());
    SetState(state_method_select);

    // TODO support SOCK5 username/password auth
    auto it = std::find(request.begin(), request.end(), socks5::no_auth_required);
    if (it == request.end()) {
      // accepted but rejected
      LOG(INFO) << "Connection (client) " << connection_id() << " socks5: no auth required.";
      return asio::error::connection_refused;
    }

    VLOG(2) << "Connection (client) " << connection_id() << " socks5 method select";
    return asio::error_code();
  }
  return asio::error::invalid_argument;
}

asio::error_code CliConnection::OnReadSocks5Handshake(std::shared_ptr<IOBuf> buf) {
  VLOG(2) << "Connection (client) " << connection_id() << " try socks5 handshake";
  socks5::request_parser parser;
  socks5::request_parser::result_type result;
  std::tie(result, std::ignore) = parser.parse(s5_request_, buf->data(), buf->data() + buf->length());

  if (result == socks5::request_parser::good) {
    DCHECK_LE(s5_request_.length(), buf->length());
    buf->trimStart(s5_request_.length());
    buf->retreat(s5_request_.length());
    SetState(state_socks5_handshake);

    VLOG(2) << "Connection (client) " << connection_id() << " socks5 handshake began";
    return asio::error_code();
  }
  return asio::error::invalid_argument;
}

asio::error_code CliConnection::OnReadSocks4Handshake(std::shared_ptr<IOBuf> buf) {
  VLOG(2) << "Connection (client) " << connection_id() << " try socks4 handshake";
  socks4::request_parser parser;
  socks4::request_parser::result_type result;
  std::tie(result, std::ignore) = parser.parse(s4_request_, buf->data(), buf->data() + buf->length());
  if (result == socks4::request_parser::good) {
    DCHECK_LE(s4_request_.length(), buf->length());
    buf->trimStart(s4_request_.length());
    buf->retreat(s4_request_.length());
    SetState(state_socks4_handshake);

    VLOG(2) << "Connection (client) " << connection_id() << " socks4 handshake began";
    return asio::error_code();
  }
  return asio::error::invalid_argument;
}

asio::error_code CliConnection::OnReadHttpRequest(std::shared_ptr<IOBuf> buf) {
  VLOG(2) << "Connection (client) " << connection_id() << " try http handshake";

  HttpRequestParser parser;

  bool ok;
  int nparsed = parser.Parse(*buf, &ok);
  if (nparsed) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " http: " << std::string_view(reinterpret_cast<const char*>(buf->data()), nparsed);
  }

  http_is_keep_alive_ = false;

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
      http_is_keep_alive_ = gurl_base::CompareCaseInsensitiveASCII(parser.connection(), "Keep-Alive"sv) == 0;
      http_keep_alive_remaining_bytes_ += parser.content_length() + header.size() - buf->length();
      VLOG(3) << "Connection (client) " << connection_id() << " Host: " << http_host_ << " Port: " << http_port_
              << " KEEPALIVE: " << std::boolalpha << http_is_keep_alive_;
      if (parser.transfer_encoding_is_chunked()) {
        // See #957
        LOG(WARNING) << "Connection (client) " << connection_id()
                     << " detected chunked transfer encoding, disabling keep alive handling";
        http_is_keep_alive_ = false;
      }
    } else {
      VLOG(3) << "Connection (client) " << connection_id() << " CONNECT: " << http_host_ << " Port: " << http_port_;
    }

    SetState(state_http_handshake);
    VLOG(2) << "Connection (client) " << connection_id() << " http handshake began";
    return asio::error_code();
  }

  LOG(WARNING) << "Connection (client) " << connection_id() << " " << parser.ErrorMessage() << ": "
               << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
  return asio::error::invalid_argument;
}

void CliConnection::ReadStream(bool yield) {
  scoped_refptr<CliConnection> self(this);
  DCHECK_EQ(downstream_read_inprogress_, false);
  if (downstream_read_inprogress_) {
    return;
  }

  if (closed_) {
    return;
  }

  downstream_read_inprogress_ = true;
  if (yield) {
    asio::post(*io_context_, [this, self]() {
      downstream_read_inprogress_ = false;
      if (closed_) {
        return;
      }
      WriteUpstreamInPipe();
      OnUpstreamWriteFlush();
    });
    return;
  }
  downlink_->async_read_some([this, self](asio::error_code ec) {
    downstream_read_inprogress_ = false;
    if (closed_) {
      return;
    }
    if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      ProcessReceivedData(nullptr, ec, 0);
      return;
    }
    WriteUpstreamInPipe();
    OnUpstreamWriteFlush();
  });
}

void CliConnection::WriteMethodSelect() {
  scoped_refptr<CliConnection> self(this);
  method_select_reply_ = socks5::method_select_response_stock_reply();
  asio::async_write(downlink_->socket_, asio::buffer(&method_select_reply_, sizeof(method_select_reply_)),
                    [this, self](asio::error_code ec, size_t bytes_transferred) {
                      if (closed_) {
                        return;
                      }
                      ProcessSentData(ec, bytes_transferred);
                    });
}

void CliConnection::WriteHandshake() {
  scoped_refptr<CliConnection> self(this);

  switch (CurrentState()) {
    case state_method_select:  // impossible
    case state_socks5_handshake:
      SetState(state_stream);
      asio::async_write(downlink_->socket_, s5_reply_.buffers(),
                        [this, self](asio::error_code ec, size_t bytes_transferred) {
                          if (closed_) {
                            return;
                          }
                          // mark eof
                          if (s5_reply_.status() != socks5::reply::request_granted) {
                            shutdown_ = true;
                            asio::error_code ec;
                            downlink_->shutdown(ec);
                          }
                          ProcessSentData(ec, bytes_transferred);
                        });
      break;
    case state_socks4_handshake:
      SetState(state_stream);
      asio::async_write(downlink_->socket_, s4_reply_.buffers(),
                        [this, self](asio::error_code ec, size_t bytes_transferred) {
                          if (closed_) {
                            return;
                          }
                          // mark eof
                          if (s4_reply_.status() != socks4::reply::request_granted) {
                            shutdown_ = true;
                            asio::error_code ec;
                            downlink_->shutdown(ec);
                          }
                          ProcessSentData(ec, bytes_transferred);
                        });
      break;
    case state_http_handshake:
      SetState(state_stream);
      /// reply on CONNECT request
      if (http_is_connect_) {
        std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(http_connect_reply_.data(), http_connect_reply_.size());
        OnDownstreamWrite(buf);
      }
      break;
    case state_error:
      break;
    case state_stream:
      break;
    default:
      LOG(FATAL) << "Connection (client) " << connection_id() << "bad state 0x" << std::hex
                 << static_cast<int>(CurrentState()) << std::dec;
  }
}

void CliConnection::WriteStream() {
  DCHECK_EQ(CurrentState(), state_stream);
  DCHECK(!write_inprogress_);
  if (UNLIKELY(write_inprogress_)) {
    return;
  }

  bool try_again = false;
  bool yield = false;

  int bytes_read_without_yielding = 0;
  uint64_t yield_after_time = GetMonotonicTime() + kYieldAfterDurationMilliseconds * 1000 * 1000;

  asio::error_code ec;
  size_t wbytes_transferred = 0u;
  do {
    if (UNLIKELY(downstream_.empty())) {
      try_again = true;
      break;
    }
    auto buf = downstream_.front();
    size_t written;
    do {
      written = downlink_->write_some(buf, ec);
      if (UNLIKELY(ec == asio::error::interrupted)) {
        continue;
      }
    } while (false);
    buf->trimStart(written);
    bytes_read_without_yielding += written;
    wbytes_transferred += written;
    // continue to resume
    if (LIKELY(buf->empty())) {
      downstream_.pop_front();
    }
    if (UNLIKELY(ec == asio::error::try_again || ec == asio::error::would_block)) {
      break;
    }
    if (UNLIKELY(ec)) {
      break;
    }
    if (UNLIKELY(!buf->empty())) {
      ec = asio::error::try_again;
      break;
    }
    if (UNLIKELY(bytes_read_without_yielding > kYieldAfterBytesRead || GetMonotonicTime() > yield_after_time)) {
      ++total_tx_yields;
      if (downstream_.empty()) {
        try_again = true;
        yield = true;
      } else {
        ec = asio::error::try_again;
      }
      break;
    }
  } while (true);

  if (try_again) {
    if (channel_ && channel_->connected() && !channel_->read_inprogress()) {
      ReadUpstreamAsync(yield);
    }
  }
  if (ec == asio::error::try_again || ec == asio::error::would_block) {
    WriteStreamAsync();

    if (!wbytes_transferred) {
      return;
    }
    ec = asio::error_code();
  }
  ProcessSentData(ec, wbytes_transferred);
}

void CliConnection::WriteStreamAsync() {
  scoped_refptr<CliConnection> self(this);
  DCHECK(!write_inprogress_);
  if (UNLIKELY(write_inprogress_)) {
    return;
  }
  write_inprogress_ = true;
  downlink_->async_write_some([this, self](asio::error_code ec) {
    write_inprogress_ = false;
    if (closed_) {
      return;
    }
    if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      ProcessSentData(ec, 0);
      return;
    }
    WriteStream();
  });
}

void CliConnection::ReadUpstream() {
  asio::error_code ec;
  size_t bytes_transferred = 0U;
  bool try_again = false;

  DCHECK(!channel_->read_inprogress());
  if (UNLIKELY(channel_->read_inprogress())) {
    return;
  }
  // DCHECK(!write_inprogress_);
  if (UNLIKELY(write_inprogress_)) {
    return;
  }

  do {
    auto buf = GetNextDownstreamBuf(ec, &bytes_transferred);
    size_t read = buf ? buf->length() : 0;
    if (UNLIKELY(ec == asio::error::try_again || ec == asio::error::would_block)) {
      ec = asio::error_code();
      try_again = true;
    } else if (UNLIKELY(ec)) {
      /* not downstream error */
      ec = asio::error_code();
      break;
    }
    if (UNLIKELY(!read)) {
      break;
    }
    WriteStream();
  } while (false);

  if (try_again) {
    if (channel_ && channel_->connected() && !channel_->read_inprogress()) {
      ReadUpstreamAsync(false);
      return;
    }
  }
}

void CliConnection::ReadUpstreamAsync(bool yield) {
  DCHECK(channel_ && channel_->connected());
  DCHECK(!channel_->read_inprogress());
  if (UNLIKELY(channel_->read_inprogress())) {
    return;
  }

  scoped_refptr<CliConnection> self(this);
  channel_->wait_read(
      [this, self](asio::error_code ec) {
        if (UNLIKELY(closed_)) {
          return;
        }
        if (UNLIKELY(ec)) {
          disconnected(ec);
          return;
        }
        received();
      },
      yield);
}

std::shared_ptr<IOBuf> CliConnection::GetNextDownstreamBuf(asio::error_code& ec, size_t* bytes_transferred) {
  if (!downstream_.empty()) {
    DCHECK(!downstream_.front()->empty());
    ec = asio::error_code();
    return downstream_.front();
  }
  if (pending_downstream_read_error_) {
    ec = std::move(pending_downstream_read_error_);
    return nullptr;
  }
  if (!channel_->connected()) {
    ec = asio::error::try_again;
    return nullptr;
  }

#ifdef HAVE_QUICHE
try_again:
#endif
  // RstStream might be sent in ProcessBytes
  if (channel_->eof()) {
    ec = asio::error::eof;
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
  } while (false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    // handled in channel_->read_some func
    // disconnected(ec);
    goto out;
  }
  if (read) {
    VLOG(2) << "Connection (client) " << connection_id() << " upstream: received reply (pipe): " << read << " bytes."
            << " done: " << channel_->rbytes_transferred() << " bytes.";
  } else {
    goto out;
  }
  *bytes_transferred += read;

#ifdef HAVE_QUICHE
  if (adapter_) {
    absl::string_view remaining_buffer(reinterpret_cast<const char*>(buf->data()), buf->length());
    while (!remaining_buffer.empty() && adapter_->want_read()) {
      http2_in_recv_callback_ = true;
      int64_t result = adapter_->ProcessBytes(remaining_buffer);
      http2_in_recv_callback_ = false;
      if (result < 0) {
        /* handled in OnConnectionError inside ProcessBytes call */
        goto out;
      }
      remaining_buffer = remaining_buffer.substr(result);
    }
    // don't want read anymore (after goaway sent)
    if (UNLIKELY(!remaining_buffer.empty())) {
      ec = asio::error::connection_refused;
      disconnected(ec);
      return nullptr;
    }
    // not enough buffer for recv window
    if (downstream_.byte_length() < H2_STREAM_WINDOW_SIZE) {
      goto try_again;
    }
  } else
#endif
      if (upstream_https_fallback_) {
    if (upstream_https_handshake_) {
      ReadUpstreamHttpsHandshake(buf, ec);
      if (ec) {
        return nullptr;
      }
    }
    if (upstream_https_chunked_) {
      ReadUpstreamHttpsChunk(buf, ec);
      if (ec) {
        return nullptr;
      }
    }
    downstream_.push_back(buf);
  } else {
    if (socks5_method_select_handshake_) {
      ReadUpstreamMethodSelectResponse(buf, ec);
      if (ec) {
        return nullptr;
      }
    }
    if (socks5_auth_handshake_) {
      ReadUpstreamAuthResponse(buf, ec);
      if (ec) {
        return nullptr;
      }
    }
    if (socks_handshake_) {
      ReadUpstreamSocksResponse(buf, ec);
      if (ec) {
        return nullptr;
      }
    }
    if (CIPHER_METHOD_IS_SOCKS(method())) {
      downstream_.push_back(buf);
    } else {
      decoder_->process_bytes(buf);
    }
  }

out:
#ifdef HAVE_QUICHE
  if (adapter_ && adapter_->want_write()) {
    // Send Control Streams
    SendIfNotProcessing();
    WriteUpstreamInPipe();
  }
#endif
  if (downstream_.empty()) {
    if (!ec) {
      ec = asio::error::try_again;
    }
    return nullptr;
  }
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    pending_downstream_read_error_ = std::move(ec);
  }
  return downstream_.front();
}

void CliConnection::ReadUpstreamHttpsHandshake(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(upstream_https_handshake_);

  upstream_https_handshake_ = false;
  HttpResponseParser parser;

  bool ok;
  int nparsed = parser.Parse(*buf, &ok);

  if (nparsed) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " http: " << std::string_view(reinterpret_cast<const char*>(buf->data()), nparsed);
  }
  if (ok && parser.status_code() == 200) {
    buf->trimStart(nparsed);
    buf->retreat(nparsed);
    if (parser.transfer_encoding_is_chunked()) {
      upstream_https_chunked_ = true;
      VLOG(1) << "Connection (client) " << connection_id() << " upstream http chunked encoding";
    }
  } else {
    if (!ok) {
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " upstream server unhandled: " << parser.ErrorMessage() << ": "
                   << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
    } else {
      LOG(WARNING) << "Connection (client) " << connection_id() << " upstream server returns: " << parser.status_code();
    }
    ec = asio::error::connection_refused;
    disconnected(ec);
    return;
  }
  if (buf->empty()) {
    ec = asio::error::try_again;
    return;
  }
}

void CliConnection::ReadUpstreamHttpsChunk(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(upstream_https_chunked_);

  HttpResponseParser parser;

  bool ok;
  int nparsed = parser.Parse(*buf, &ok);

  if (nparsed) {
    VLOG(3) << "Connection (client) " << connection_id()
            << " chunked http: " << std::string_view(reinterpret_cast<const char*>(buf->data()), nparsed);
  }
  if (ok && parser.status_code() == 200) {
    buf->trimStart(nparsed);
    buf->retreat(nparsed);
    upstream_https_chunked_ = false;
    if (parser.content_length() != 0) {
      LOG(WARNING) << "Connection (client) " << connection_id() << " upstream server returns unexpected body";
      ec = asio::error::invalid_argument;
      return;
    }
    if (buf->empty()) {
      ec = asio::error::try_again;
      return;
    }
  } else {
    if (!ok) {
      LOG(WARNING) << "Connection (client) " << connection_id()
                   << " upstream server unhandled: " << parser.ErrorMessage() << ": "
                   << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
    } else {
      LOG(WARNING) << "Connection (client) " << connection_id() << " upstream server returns: " << parser.status_code();
    }
    ec = asio::error::connection_refused;
    return;
  }
}

void CliConnection::WriteUpstreamMethodSelectRequest() {
  socks5::method_select_request_header method_select_header;
  method_select_header.ver = socks5::version;
  method_select_header.nmethods = 1;  // we only support auth or non-auth but not all of them.

  bool auth_required = !absl::GetFlag(FLAGS_username).empty() && !absl::GetFlag(FLAGS_password).empty();

  ByteRange req(reinterpret_cast<const uint8_t*>(&method_select_header), sizeof(method_select_header));
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
  buf->reserve(0, sizeof(uint8_t));
  *buf->mutable_tail() = auth_required ? socks5::username_or_password : socks5::no_auth_required;
  buf->append(sizeof(uint8_t));

  upstream_.push_back(buf);
  socks5_method_select_handshake_ = true;
  socks5_auth_handshake_ = auth_required;
  if (auth_required) {
    /* alternative for socks5 auth request */
    upstream_.push_back(IOBuf::create(SOCKET_DEBUF_SIZE));
  }
  /* alternative for socks5 request */
  upstream_.push_back(IOBuf::create(SOCKET_DEBUF_SIZE));
}

void CliConnection::ReadUpstreamMethodSelectResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(socks5_method_select_handshake_);
  socks5_method_select_handshake_ = false;
  auto response = reinterpret_cast<const socks5::method_select_response*>(buf->data());
  if (buf->length() < sizeof(socks5::method_select_response)) {
    LOG(INFO) << "Connection (client) " << connection_id()
              << " malformed upstream socks5 method select handshake response.";
    goto err_out;
  }
  if (response->ver != socks5::version) {
    LOG(INFO) << "Connection (client) " << connection_id()
              << " malformed upstream socks5 method select handshake response.";
    goto err_out;
  }
  if (!socks5_auth_handshake_ && response->method != socks5::no_auth_required) {
    LOG(INFO) << "Connection (server) " << connection_id() << " upstream socks5: noauth required.";
    goto err_out;
  }
  if (socks5_auth_handshake_ && response->method != socks5::username_or_password) {
    LOG(INFO) << "Connection (server) " << connection_id() << " upstream socks5: auth required.";
    goto err_out;
  }
  VLOG(2) << "Connection (client) " << connection_id() << " upstream socks5 method select response "
          << (socks5_auth_handshake_ ? "(auth)" : "(noauth)");
  buf->trimStart(sizeof(socks5::method_select_response));
  buf->retreat(sizeof(socks5::method_select_response));

  if (socks5_auth_handshake_) {
    WriteUpstreamAuthRequest();
  } else {
    WriteUpstreamSocks5Request();
  }

  if (buf->empty()) {
    ec = asio::error::try_again;
    return;
  }

  return;

err_out:
  ec = asio::error::connection_refused;
  disconnected(ec);
  return;
}

void CliConnection::WriteUpstreamAuthRequest() {
  DCHECK(CIPHER_METHOD_IS_SOCKS5(method()));
  socks5::auth_request_header header;
  header.ver = socks5::version;
  std::string username = absl::GetFlag(FLAGS_username);
  std::string password = absl::GetFlag(FLAGS_password);

  ByteRange req(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
  buf->reserve(0, sizeof(uint8_t));
  *buf->mutable_tail() = username.size();
  buf->append(sizeof(uint8_t));

  buf->reserve(0, username.size());
  memcpy(buf->mutable_tail(), username.c_str(), username.size());
  buf->append(username.size());

  buf->reserve(0, sizeof(uint8_t));
  *buf->mutable_tail() = password.size();
  buf->append(sizeof(uint8_t));

  buf->reserve(0, password.size());
  memcpy(buf->mutable_tail(), password.c_str(), password.size());
  buf->append(password.size());

  upstream_.replace_front(buf);
  WriteUpstreamInPipe();
}

void CliConnection::ReadUpstreamAuthResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(socks5_auth_handshake_);
  socks5_auth_handshake_ = false;
  auto response = reinterpret_cast<const socks5::auth_response*>(buf->data());
  if (buf->length() < sizeof(socks5::auth_response)) {
    LOG(INFO) << "Connection (client) " << connection_id() << " malformed upstream socks5 auth response.";
    goto err_out;
  }
  if (response->ver != socks5::version) {
    LOG(INFO) << "Connection (client) " << connection_id() << " malformed upstream socks5 auth response.";
    goto err_out;
  }
  if (response->status != socks5::auth_response_status::success) {
    LOG(INFO) << "Connection (client) " << connection_id() << " rejected in upstream auth handshake response.";
    goto err_out;
  }
  VLOG(2) << "Connection (client) " << connection_id() << " upstream socks5 auth response";
  buf->trimStart(sizeof(socks5::auth_response));
  buf->retreat(sizeof(socks5::auth_response));

  WriteUpstreamSocks5Request();

  if (buf->empty()) {
    ec = asio::error::try_again;
    return;
  }

  return;

err_out:
  ec = asio::error::connection_refused;
  disconnected(ec);
  return;
}

void CliConnection::WriteUpstreamSocks4Request() {
  bool auth_required = !absl::GetFlag(FLAGS_username).empty() && !absl::GetFlag(FLAGS_password).empty();
  if (auth_required) {
    LOG(WARNING) << "Client specifies username and password but SOCKS4 doesn't support it";
  }
  socks4::request_header header;
  header.version = socks4::version;
  header.command = socks4::cmd_connect;
  header.port_high_byte = ss_request_->port_high_byte();
  header.port_low_byte = ss_request_->port_low_byte();
  if (ss_request_->address_type() == ss::domain) {
    // impossible
    CHECK(false);
  } else if (ss_request_->address_type() == ss::ipv6) {
    // not supported
    LOG(WARNING) << "Unsupported IPv6 address for SOCKS4 server";
    OnDisconnect(asio::error::access_denied);
    return;
  } else {
    memcpy(&header.address, &ss_request_->address4(), sizeof(header.address));
  }

  ByteRange req(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
  // append userid (variable)
  buf->reserve(0, sizeof(uint8_t));
  *buf->mutable_tail() = '\0';
  buf->append(sizeof(uint8_t));

  socks_handshake_ = true;

  upstream_.push_back(buf);
}

void CliConnection::WriteUpstreamSocks4ARequest() {
  bool auth_required = !absl::GetFlag(FLAGS_username).empty() && !absl::GetFlag(FLAGS_password).empty();
  if (auth_required) {
    LOG(WARNING) << "Client specifies username and password but SOCKS4A doesn't support it";
  }
  socks4::request_header header;
  header.version = socks4::version;
  header.command = socks4::cmd_connect;
  header.port_high_byte = ss_request_->port_high_byte();
  header.port_low_byte = ss_request_->port_low_byte();
  std::string domain_name;
  if (ss_request_->address_type() == ss::domain) {
    domain_name = ss_request_->domain_name();
  } else if (ss_request_->address_type() == ss::ipv6) {
    asio::ip::address_v6 address(ss_request_->address6());
    domain_name = address.to_string();
  } else {
    asio::ip::address_v4 address(ss_request_->address4());
    domain_name = address.to_string();
  }
  uint32_t address = 0x0f << 24;  // marked as SOCKS4A
  memcpy(&header.address, &address, sizeof(address));

  ByteRange req(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
  // append userid (variable)
  buf->reserve(0, sizeof(uint8_t));
  *buf->mutable_tail() = '\0';
  buf->append(sizeof(uint8_t));
  buf->reserve(0, domain_name.size() + sizeof(uint8_t));
  memcpy(buf->mutable_tail(), domain_name.c_str(), domain_name.size() + sizeof(uint8_t));
  buf->append(domain_name.size() + sizeof(uint8_t));

  socks_handshake_ = true;

  upstream_.push_back(buf);
}

void CliConnection::WriteUpstreamSocks5Request() {
  socks5::request_header header;
  header.version = socks5::version;
  header.command = socks5::cmd_connect;
  header.null_byte = 0;

  ByteRange req(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);

  span<const uint8_t> address;
  std::string domain_name;

  uint8_t address_type = socks5::ipv4;
  if (ss_request_->address_type() == ss::domain) {
    address_type = socks5::domain;
    domain_name = ss_request_->domain_name();
    address = as_bytes(make_span(domain_name));
  } else if (ss_request_->address_type() == ss::ipv6) {
    address_type = socks5::ipv6;
    address = as_bytes(make_span(ss_request_->address6()));
  } else {
    address_type = socks5::ipv4;
    address = as_bytes(make_span(ss_request_->address4()));
  }

  buf->reserve(0, sizeof(address_type));
  memcpy(buf->mutable_tail(), &address_type, sizeof(address_type));
  buf->append(sizeof(address_type));

  if (ss_request_->address_type() == ss::domain) {
    uint8_t address_len = address.size();
    buf->reserve(0, sizeof(address_len));
    memcpy(buf->mutable_tail(), &address_len, sizeof(address_len));
    buf->append(sizeof(address_len));
  }

  buf->reserve(0, address.size());
  memcpy(buf->mutable_tail(), address.data(), address.size());
  buf->append(address.size());

  uint8_t port_high_byte = ss_request_->port_high_byte();
  uint8_t port_low_byte = ss_request_->port_low_byte();

  buf->reserve(0, sizeof(uint16_t));
  *buf->mutable_tail() = port_high_byte;
  buf->append(sizeof(port_high_byte));
  *buf->mutable_tail() = port_low_byte;
  buf->append(sizeof(port_low_byte));

  socks_handshake_ = true;

  upstream_.replace_front(buf);
  WriteUpstreamInPipe();
}

void CliConnection::ReadUpstreamSocksResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(socks_handshake_);
  socks_handshake_ = false;
  switch (method()) {
    case CRYPTO_SOCKS4:
    case CRYPTO_SOCKS4A: {
      if (buf->length() < sizeof(socks4::reply_header)) {
        goto err_out;
      }
      auto response = reinterpret_cast<const socks4::reply_header*>(buf->data());
      if (response->null_byte != 0 || response->status != socks4::reply::request_granted) {
        goto err_out;
      }
      VLOG(2) << "Connection (client) " << connection_id() << " upstream socks4 handshake response";
      buf->trimStart(sizeof(socks4::reply_header));
      buf->retreat(sizeof(socks4::reply_header));
      break;
    };
    case CRYPTO_SOCKS5:
    case CRYPTO_SOCKS5H: {
      if (buf->length() < sizeof(socks5::reply_header)) {
        goto err_out;
      }
      auto response = reinterpret_cast<const socks5::reply_header*>(buf->data());
      if (response->version != socks5::version || response->status != socks5::reply::request_granted ||
          response->null_byte != 0) {
        goto err_out;
      }
      if (response->address_type == socks5::ipv4) {
        uint32_t expected_len =
            sizeof(socks5::reply_header) + sizeof(asio::ip::address_v4::bytes_type) + sizeof(uint16_t);
        if (buf->length() < expected_len) {
          goto err_out;
        }
        buf->trimStart(expected_len);
        buf->retreat(expected_len);
      } else if (response->address_type == socks5::ipv6) {
        uint32_t expected_len =
            sizeof(socks5::reply_header) + sizeof(asio::ip::address_v6::bytes_type) + sizeof(uint16_t);
        if (buf->length() < expected_len) {
          goto err_out;
        }
        buf->trimStart(expected_len);
        buf->retreat(expected_len);
      } else if (response->address_type == socks5::domain) {
        uint32_t expected_len = sizeof(socks5::reply_header) + sizeof(uint8_t) + sizeof(uint16_t);
        if (buf->length() < expected_len) {
          goto err_out;
        }
        expected_len += *reinterpret_cast<const uint8_t*>(buf->data() + sizeof(*response));
        if (buf->length() < expected_len) {
          goto err_out;
        }
        buf->trimStart(expected_len);
        buf->retreat(expected_len);
      } else {
        goto err_out;
        return;
      }
      VLOG(2) << "Connection (client) " << connection_id() << " upstream socks5 handshake response";
      break;
    };
    default:
      CHECK(false);
      break;
  }
  if (buf->empty()) {
    ec = asio::error::try_again;
    return;
  }
  return;

err_out:
  LOG(WARNING) << "Connection (client) " << connection_id() << " malformed upstream socks handshake response";
  ec = asio::error::connection_refused;
  disconnected(ec);
  return;
}

void CliConnection::WriteUpstreamInPipe() {
  size_t bytes_transferred = 0U, wbytes_transferred = 0U;
  bool try_again = false;
  bool yield = false;

  int bytes_read_without_yielding = 0;
  uint64_t yield_after_time = GetMonotonicTime() + kYieldAfterDurationMilliseconds * 1000 * 1000;

  if (UNLIKELY(channel_ && channel_->write_inprogress())) {
    return;
  }

  asio::error_code ec;

  /* recursively send the remainings */
  while (true) {
    size_t read;
    bool upstream_blocked;
    std::shared_ptr<IOBuf> buf = GetNextUpstreamBuf(ec, &bytes_transferred, &upstream_blocked);

    read = buf ? buf->length() : 0;

    if (UNLIKELY(ec == asio::error::try_again || ec == asio::error::would_block)) {
      if (!upstream_blocked) {
        ec = asio::error_code();
        try_again = true;
      }
    } else if (UNLIKELY(ec)) {
      /* handled in getter */
      return;
    }
    if (UNLIKELY(!read)) {
      break;
    }
    if (UNLIKELY(!channel_ || !channel_->connected() || channel_->eof())) {
      ec = asio::error::try_again;
      break;
    }
    ec = asio::error_code();
    size_t written;
    do {
      written = channel_->write_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while (false);
    buf->trimStart(written);
    wbytes_transferred += written;
    bytes_read_without_yielding += written;
    if (UNLIKELY(ec == asio::error::try_again || ec == asio::error::would_block)) {
      DCHECK_EQ(0u, written);
      break;
    }
    VLOG(2) << "Connection (client) " << connection_id() << " upstream: sent request (pipe): " << written << " bytes"
            << " done: " << channel_->wbytes_transferred() << " bytes."
            << " ec: " << ec;
    // continue to resume
    if (UNLIKELY(buf->empty())) {
      DCHECK(!upstream_.empty() && upstream_.front() == buf);
      upstream_.pop_front();
    }
    if (UNLIKELY(ec)) {
      OnDisconnect(ec);
      return;
    }
    if (UNLIKELY(!buf->empty())) {
      ec = asio::error::try_again;
      break;
    }
    if (UNLIKELY(bytes_read_without_yielding > kYieldAfterBytesRead || GetMonotonicTime() > yield_after_time)) {
      ++total_rx_yields;
      if (upstream_.empty()) {
        try_again = true;
        yield = true;
      } else {
        ec = asio::error::try_again;
      }
      break;
    }
  }
  if (try_again) {
    if (!downstream_read_inprogress_) {
      ReadStream(yield);
    }
  }
  if (ec == asio::error::try_again || ec == asio::error::would_block) {
    OnUpstreamWriteFlush();
    return;
  }
}

std::shared_ptr<IOBuf> CliConnection::GetNextUpstreamBuf(asio::error_code& ec,
                                                         size_t* bytes_transferred,
                                                         bool* upstream_blocked) {
  *upstream_blocked = false;
  if (!upstream_.empty()) {
    // pending on upstream handshake
    if ((socks5_method_select_handshake_ || socks5_auth_handshake_) && upstream_.front()->empty()) {
      ec = asio::error::try_again;
      return nullptr;
    }
    DCHECK(!upstream_.front()->empty());
    ec = asio::error_code();
    return upstream_.front();
  }
  if (pending_upstream_read_error_) {
    ec = std::move(pending_upstream_read_error_);
    return nullptr;
  }
  // RstStream might be sent in ProcessBytes
  if (closed_) {
    ec = asio::error::eof;
    return nullptr;
  }

  std::shared_ptr<IOBuf> buf;
  size_t read;

#ifdef HAVE_QUICHE
  if (data_frame_ && !data_frame_->empty()) {
    VLOG(2) << "Connection (client) " << connection_id() << " has pending data to send upstream, defer reading";
    *upstream_blocked = true;
    ec = asio::error::try_again;
    goto out;
  }
#endif

  do {
    buf = IOBuf::create(SOCKET_BUF_SIZE);
    read = downlink_->socket_.read_some(tail_buffer(*buf, SOCKET_BUF_SIZE), ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while (false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    /* safe to return, socket will handle this error later */
    ProcessReceivedData(nullptr, ec, 0);
    goto out;
  }
  rbytes_transferred_ += read;
  total_rx_bytes += read;
  ++total_rx_times;
  *bytes_transferred += read;
  if (read) {
    VLOG(2) << "Connection (client) " << connection_id() << " received data (pipe): " << read << " bytes."
            << " done: " << rbytes_transferred_ << " bytes.";
  } else {
    goto out;
  }

  if (!channel_ || !channel_->connected()) {
    OnStreamRead(buf);
    ec = asio::error::try_again;
    return nullptr;
  }

  if (http_is_keep_alive_) {
    if (http_keep_alive_remaining_bytes_ < (int64_t)read) {
      VLOG(1) << "Connection (client) " << connection_id() << " reused for keep-alive connection";
      // currently, we assume the host doesn't change
      ec = OnReadHttpRequest(buf);
      SetState(state_stream);
      if (ec) {
        return nullptr;
      }
    } else {
      http_keep_alive_remaining_bytes_ -= read;
    }
  }

#ifdef HAVE_QUICHE
  if (adapter_) {
    if (!data_frame_) {
      ec = asio::error::eof;
      return nullptr;
    }
    if (padding_support_ && num_padding_send_ < kFirstPaddings) {
      ++num_padding_send_;
      AddPadding(buf);
    }
    data_frame_->AddChunk(buf);
  } else
#endif
      if (upstream_https_fallback_) {
    upstream_.push_back(buf);
  } else {
    if (CIPHER_METHOD_IS_SOCKS(method())) {
      upstream_.push_back(buf);
    } else {
      EncryptData(&upstream_, buf);
    }
  }

out:
#ifdef HAVE_QUICHE
  if (data_frame_) {
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter_->ResumeStream(stream_id_);
    SendIfNotProcessing();
  }
#endif
  if (upstream_.empty()) {
    if (read) {
      *upstream_blocked = true;
    }
    if (!ec) {
      ec = asio::error::try_again;
    }
    return nullptr;
  }
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    pending_upstream_read_error_ = std::move(ec);
  }
  return upstream_.front();
}

asio::error_code CliConnection::PerformCmdOpsV5(const socks5::request* request, socks5::reply* reply) {
  asio::error_code ec;

  switch (request->command()) {
    case socks5::cmd_connect: {
      if (request->port() == 0u || (request->address_type() == socks5::domain && request->domain_name().empty()) ||
          (request->address_type() != socks5::domain && request->endpoint().address().is_unspecified())) {
        LOG(WARNING) << "Connection (client) " << connection_id() << " socks5: requested invalid port or empty host";
        reply->mutable_status() = socks5::reply::request_failed_network_unreachable;
        ec = asio::error::invalid_argument;
        break;
      }

      asio::ip::tcp::endpoint endpoint;
      if (request->address_type() == socks5::domain) {
        endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0);
      } else {
        endpoint = request->endpoint();
      }
      reply->set_endpoint(endpoint);
      reply->mutable_status() = socks5::reply::request_granted;

      if (request->address_type() == socks5::domain) {
        DCHECK_LE(request->domain_name().size(), (unsigned int)TLSEXT_MAXLEN_host_name);
        OnCmdConnect(request->domain_name(), request->port());
      } else {
        OnCmdConnect(request->endpoint());
      }
    } break;
    case socks5::cmd_bind:
    case socks5::cmd_udp_associate:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id() << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks5::reply::request_failed_cmd_not_supported;
      ec = asio::error::invalid_argument;
      break;
  }
  return ec;
}

asio::error_code CliConnection::PerformCmdOpsV4(const socks4::request* request, socks4::reply* reply) {
  asio::error_code ec;

  switch (request->command()) {
    case socks4::cmd_connect: {
      if (request->port() == 0u || (request->is_socks4a() && request->domain_name().empty()) ||
          (!request->is_socks4a() && request->endpoint().address().is_unspecified())) {
        LOG(WARNING) << "Connection (client) " << connection_id() << " socks4: requested invalid port or empty host";
        reply->mutable_status() = socks4::reply::request_failed;
        ec = asio::error::invalid_argument;
        break;
      }

      asio::ip::tcp::endpoint endpoint{asio::ip::tcp::v4(), 0};
      reply->set_endpoint(endpoint);
      reply->mutable_status() = socks4::reply::request_granted;

      if (request->is_socks4a()) {
        if (request->domain_name().size() > TLSEXT_MAXLEN_host_name) {
          LOG(WARNING) << "Connection (client) " << connection_id()
                       << " socks4a: too long domain name: " << request->domain_name();
          reply->mutable_status() = socks4::reply::request_failed;
          ec = asio::error::invalid_argument;
          break;
        }

        OnCmdConnect(request->domain_name(), request->port());
      } else {
        OnCmdConnect(request->endpoint());
      }
    } break;
    case socks4::cmd_bind:
    default:
      // NOT IMPLETMENTED
      LOG(WARNING) << "Connection (client) " << connection_id() << " not supported command 0x" << std::hex
                   << static_cast<int>(request->command()) << std::dec;
      reply->mutable_status() = socks4::reply::request_failed;
      ec = asio::error::invalid_argument;
      break;
  }
  return ec;
}

asio::error_code CliConnection::PerformCmdOpsHttp() {
  if (http_host_.size() > TLSEXT_MAXLEN_host_name) {
    LOG(WARNING) << "Connection (client) " << connection_id() << " http: too long domain name: " << http_host_;
    return asio::error::invalid_argument;
  }

  if (http_host_.empty() || http_port_ == 0u) {
    LOG(WARNING) << "Connection (client) " << connection_id() << " https: requested empty host or invalid port";
    return asio::error::invalid_argument;
  }

  OnCmdConnect(http_host_, http_port_);

  return asio::error_code();
}

void CliConnection::ProcessReceivedData(std::shared_ptr<IOBuf> buf, asio::error_code ec, size_t bytes_transferred) {
  VLOG(2) << "Connection (client) " << connection_id() << " received data: " << bytes_transferred << " bytes"
          << " done: " << rbytes_transferred_ << " bytes."
          << " ec: " << ec;

  rbytes_transferred_ += bytes_transferred;
  total_rx_bytes += bytes_transferred;
  ++total_rx_times;

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
        if (ec) {
          // ready to read eof after send handshake
          ReadStream(true);
          return;
        }
        VLOG(2) << "Connection (client) " << connection_id() << " socks5 handshake finished";
        goto handle_stream;
      case state_socks4_handshake:
        ec = PerformCmdOpsV4(&s4_request_, &s4_reply_);
        WriteHandshake();
        if (ec) {
          // ready to read eof after send handshake
          ReadStream(true);
          return;
        }
        VLOG(2) << "Connection (client) " << connection_id() << " socks4 handshake finished";
        goto handle_stream;
      case state_http_handshake:
        ec = PerformCmdOpsHttp();
        if (ec) {
          break;
        }
        WriteHandshake();
        VLOG(2) << "Connection (client) " << connection_id() << " http handshake finished";
        goto handle_stream;
      case state_stream:
      handle_stream:
        if (buf->length()) {
          OnStreamRead(buf);
          return;
        }
        WriteUpstreamInPipe();  // continously read
        OnUpstreamWriteFlush();
        break;
      case state_error:
        ec = asio::error::invalid_argument;
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id() << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    };
  }
  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
}

void CliConnection::ProcessSentData(asio::error_code ec, size_t bytes_transferred) {
  wbytes_transferred_ += bytes_transferred;
  total_tx_bytes += bytes_transferred;
  ++total_tx_times;

  VLOG(2) << "Connection (client) " << connection_id() << " sent data: " << bytes_transferred << " bytes."
          << " done: " << wbytes_transferred_ << " bytes."
          << " ec: " << ec;

  if (!ec) {
    switch (CurrentState()) {
      case state_method_select:
        ReadSocks5Handshake();  // read next state info
        break;
      case state_socks5_handshake:
      case state_socks4_handshake:
      case state_http_handshake:
        ec = asio::error::invalid_argument;
        break;
      case state_stream:
        if (bytes_transferred) {
          OnStreamWrite();
        }
        break;
      case state_error:
        ec = asio::error::invalid_argument;
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id() << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    }
  }

  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
}

void CliConnection::OnCmdConnect(const asio::ip::tcp::endpoint& endpoint) {
  DCHECK(!endpoint.address().is_unspecified());
  DCHECK_NE(0u, endpoint.port());
  ss_request_ = std::make_unique<ss::request>(endpoint);
  OnConnect();
}

void CliConnection::OnCmdConnect(const std::string& domain_name, uint16_t port) {
  DCHECK_LE(domain_name.size(), (unsigned int)TLSEXT_MAXLEN_host_name);

  if (CIPHER_METHOD_IS_SOCKS_NON_DOMAIN_NAME(method())) {
    VLOG(1) << "Connection (client) " << connection_id() << " resolving domain name " << domain_name << " locally";
    scoped_refptr<CliConnection> self(this);
    int ret = resolver_.Init();
    if (ret < 0) {
      LOG(WARNING) << "resolver initialize failure";
      OnDisconnect(asio::error::host_not_found);
      return;
    }
    resolver_.AsyncResolve(
        domain_name, port,
        [this, self, domain_name](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
          resolver_.Reset();
          // Cancelled, safe to ignore
          if (UNLIKELY(ec == asio::error::operation_aborted)) {
            return;
          }
          if (closed_) {
            return;
          }
          if (ec) {
            OnDisconnect(ec);
            return;
          }
          asio::ip::tcp::endpoint endpoint;
          for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
            endpoint = iter->endpoint();
            break;
          }
          if (endpoint.address().is_unspecified() || endpoint.port() == 0u) {
            LOG(WARNING) << "Connection (client) " << connection_id() << " failed to resolve domain name "
                         << domain_name;
            OnDisconnect(asio::error::invalid_argument);
            return;
          }
          VLOG(1) << "Connection (client) " << connection_id() << " resolved domain name " << domain_name << " to "
                  << endpoint.address();
          ss_request_ = std::make_unique<ss::request>(endpoint);
          OnConnect();
        });
    return;
  }
  DCHECK(!domain_name.empty());
  DCHECK_NE(0u, port);
  ss_request_ = std::make_unique<ss::request>(domain_name, port);
  OnConnect();
}

void CliConnection::OnConnect() {
  scoped_refptr<CliConnection> self(this);
  LOG(INFO) << "Connection (client) " << connection_id() << " connect " << remote_domain();
  // create lazy
  if (enable_upstream_tls_) {
    channel_ = ssl_stream::create(ssl_socket_data_index(), *io_context_, remote_host_ips_, remote_host_sni_,
                                  remote_port_, this, upstream_https_fallback_, upstream_ssl_ctx_);

  } else {
    channel_ = stream::create(*io_context_, remote_host_ips_, remote_host_sni_, remote_port_, this);
  }
  channel_->async_connect([this, self](asio::error_code ec) {
    if (UNLIKELY(closed_)) {
      return;
    }
    if (UNLIKELY(ec)) {
      disconnected(ec);
      return;
    }
    connected();
  });
}

void CliConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  if (!channel_ || !channel_->connected()) {
    constexpr const size_t kMaxHeaderSize = 1024 * 1024 + 1024;
    if (pending_data_.byte_length() + buf->length() > kMaxHeaderSize) {
      LOG(WARNING) << "Connection (client) " << connection_id() << " too much data in incoming";
      OnDisconnect(asio::error::connection_reset);
      return;
    }
    pending_data_.push_back(buf);
    return;
  }

#ifdef HAVE_QUICHE
  // SendContents
  if (adapter_) {
    if (!data_frame_) {
      return;
    }
    if (padding_support_ && num_padding_send_ < kFirstPaddings) {
      ++num_padding_send_;
      AddPadding(buf);
    }
    data_frame_->AddChunk(buf);
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter()->ResumeStream(stream_id_);
    SendIfNotProcessing();
  } else
#endif
      if (upstream_https_fallback_) {
    upstream_.push_back(buf);
  } else {
    if (CIPHER_METHOD_IS_SOCKS(method())) {
      upstream_.push_back(buf);
    } else {
      EncryptData(&upstream_, buf);
    }
  }
  OnUpstreamWriteFlush();
}

void CliConnection::OnStreamWrite() {
  OnDownstreamWriteFlush();

  /* shutdown the socket if upstream is eof and all remaining data sent */
  if (channel_ && channel_->eof() && downstream_.empty() && !shutdown_) {
    VLOG(2) << "Connection (client) " << connection_id() << " last data sent: shutting down";
    shutdown_ = true;
    asio::error_code ec;
    downlink_->shutdown(ec);
    return;
  }
}

void CliConnection::OnDisconnect(asio::error_code ec) {
#ifdef WIN32
  if (ec.value() == WSAESHUTDOWN) {
    ec = asio::error_code();
  }
#else
  if (ec.value() == asio::error::operation_aborted) {
    ec = asio::error_code();
  }
#endif
  LOG(INFO) << "Connection (client) " << connection_id() << " closed: " << ec;
  close();
}

void CliConnection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void CliConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf) {
    DCHECK(!buf->empty());
    downstream_.push_back(buf);
  }
  if (!downstream_.empty() && !write_inprogress_) {
    if (CurrentState() == state_error) {
      VLOG(1) << "Connection (client) " << connection_id() << " failed to sending " << (buf ? buf->length() : 0u)
              << " bytes.";
      return;
    }
    WriteStream();
  }
}

void CliConnection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void CliConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    VLOG(2) << "Connection (client) " << connection_id() << " upstream: ready to send request: " << buf->length()
            << " bytes.";
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    scoped_refptr<CliConnection> self(this);
    channel_->wait_write([this, self](asio::error_code ec) {
      if (UNLIKELY(closed_)) {
        return;
      }
      if (UNLIKELY(ec)) {
        disconnected(ec);
        return;
      }
      sent();
    });
  }
}

void CliConnection::connected() {
  scoped_refptr<CliConnection> self(this);
  VLOG(2) << "Connection (client) " << connection_id()
          << " remote: established upstream connection with: " << remote_domain();

  bool http2 = CIPHER_METHOD_IS_HTTP2(method());
  if (http2 && channel_->https_fallback()) {
    http2 = false;
    upstream_https_fallback_ = true;
  }

  // Create adapters
#ifdef HAVE_QUICHE
  if (http2) {
#ifdef HAVE_NGHTTP2
    adapter_ = http2::adapter::NgHttp2Adapter::CreateClientAdapter(*this);
#else
    http2::adapter::OgHttp2Adapter::Options options;
    options.perspective = http2::adapter::Perspective::kClient;
    adapter_ = http2::adapter::OgHttp2Adapter::Create(*this, options);
#endif
    padding_support_ = absl::GetFlag(FLAGS_padding_support);
  } else
#endif
      if (upstream_https_fallback_) {
    // nothing to create
    // TODO should we support it?
    // padding_support_ = absl::GetFlag(FLAGS_padding_support);
  } else {
    DCHECK(!http2);
    if (!CIPHER_METHOD_IS_SOCKS(method())) {
      encoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password), method(), this, true);
      decoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password), method(), this);
    }
  }

#ifdef HAVE_QUICHE
  // Send Upstream Settings (HTTP2 Only)
  if (adapter_) {
    std::vector<http2::adapter::Http2Setting> settings{
        {http2::adapter::Http2KnownSettingsId::HEADER_TABLE_SIZE, kSpdyMaxHeaderTableSize},
        {http2::adapter::Http2KnownSettingsId::MAX_CONCURRENT_STREAMS, kSpdyMaxConcurrentPushedStreams},
        {http2::adapter::Http2KnownSettingsId::INITIAL_WINDOW_SIZE, H2_STREAM_WINDOW_SIZE},
        {http2::adapter::Http2KnownSettingsId::MAX_HEADER_LIST_SIZE, kSpdyMaxHeaderListSize},
        {http2::adapter::Http2KnownSettingsId::ENABLE_PUSH, kSpdyDisablePush},
    };
    adapter_->SubmitSettings(settings);
    SendIfNotProcessing();
  }

  // Send Upstream Header
  if (adapter_) {
    std::string hostname_and_port;
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
    hostname_and_port = absl::StrCat(host, ":", port);

    // Handle IPv6 literals.
    asio::error_code ec;
    auto addr = asio::ip::make_address(host, ec);
    if (!ec && addr.is_v6()) {
      hostname_and_port = absl::StrCat("[", host, "]", ":", port);
    }

    std::unique_ptr<DataFrameSource> data_frame = std::make_unique<DataFrameSource>(this);
    data_frame_ = data_frame.get();
    std::vector<std::pair<std::string, std::string>> headers;
    headers.emplace_back(":method"s, "CONNECT"s);
    //    authority   = [ userinfo "@" ] host [ ":" port ]
    headers.emplace_back(":authority"s, hostname_and_port);
    headers.emplace_back("host"s, hostname_and_port);
    bool auth_required = !absl::GetFlag(FLAGS_username).empty() && !absl::GetFlag(FLAGS_password).empty();
    if (auth_required) {
      headers.emplace_back("proxy-authorization"s, absl::StrCat("basic ", GetProxyAuthorizationIdentity()));
    }
    // Send "Padding" header
    // originated from naive_proxy_delegate.go;func ServeHTTP
    if (padding_support_) {
      // Sends client-side padding header regardless of server support
      std::string padding(gurl_base::RandInt(16, 32), '~');
      InitializeNonindexCodes();
      FillNonindexHeaderValue(gurl_base::RandUint64(), &padding[0], padding.size());
      headers.emplace_back("padding"s, padding);
    }
    int submit_result = adapter_->SubmitRequest(GenerateHeaders(headers), std::move(data_frame), false, nullptr);
    if (submit_result < 0) {
      adapter_->SubmitGoAway(0, http2::adapter::Http2ErrorCode::INTERNAL_ERROR, ""sv);
    } else {
      stream_id_ = submit_result;
      data_frame_->set_stream_id(stream_id_);
    }
    SendIfNotProcessing();
  } else
#endif
      if (upstream_https_fallback_) {
    std::string hostname_and_port;
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
    hostname_and_port = absl::StrCat(host, ":", port);

    // Handle IPv6 literals.
    asio::error_code ec;
    auto addr = asio::ip::make_address(host, ec);
    if (!ec && addr.is_v6()) {
      hostname_and_port = absl::StrCat("[", host, "]", ":", port);
    }

    bool auth_required = !absl::GetFlag(FLAGS_username).empty() && !absl::GetFlag(FLAGS_password).empty();

    std::string hdr = absl::StrFormat(
        "CONNECT %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Proxy-Authorization: %s\r\n"
        "Proxy-Connection: Close\r\n"
        "\r\n",
        hostname_and_port.c_str(), hostname_and_port.c_str(), absl::StrCat("basic ", GetProxyAuthorizationIdentity()));
    if (!auth_required) {
      hdr = absl::StrFormat(
          "CONNECT %s HTTP/1.1\r\n"
          "Host: %s\r\n"
          "Proxy-Connection: Close\r\n"
          "\r\n",
          hostname_and_port.c_str(), hostname_and_port.c_str());
    }
    // write variable address directly as https header
    upstream_.push_back(hdr.data(), hdr.size());
  } else {
    if (CIPHER_METHOD_IS_SOCKS(method())) {
      switch (method()) {
        case CRYPTO_SOCKS4:
          WriteUpstreamSocks4Request();
          if (closed_) {
            return;
          }
          break;
        case CRYPTO_SOCKS4A:
          WriteUpstreamSocks4ARequest();
          break;
        case CRYPTO_SOCKS5:
        case CRYPTO_SOCKS5H:
          WriteUpstreamMethodSelectRequest();
          break;
        default:
          CHECK(false);
          break;
      }
    } else {
      ByteRange req(ss_request_->data(), ss_request_->length());
      std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(req);
      // write variable address directly as ss header
      EncryptData(&upstream_, buf);
    }
  }

  // Re-process the read data in pending
  if (!pending_data_.empty()) {
    auto queue = std::move(pending_data_);
    while (!queue.empty()) {
      auto buf = queue.front();
      queue.pop_front();
      OnStreamRead(buf);
    }
    WriteUpstreamInPipe();
  }

  upstream_readable_ = true;
  upstream_writable_ = true;

  ReadUpstream();
  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();
}

void CliConnection::received() {
  scoped_refptr<CliConnection> self(this);
  ReadUpstream();
}

void CliConnection::sent() {
  scoped_refptr<CliConnection> self(this);

  upstream_writable_ = true;

  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();

#ifdef HAVE_QUICHE
  if (blocked_stream_) {
    adapter_->ResumeStream(blocked_stream_);
    SendIfNotProcessing();
    OnUpstreamWriteFlush();
  }
#endif
}

void CliConnection::disconnected(asio::error_code ec) {
  scoped_refptr<CliConnection> self(this);
  VLOG(1) << "Connection (client) " << connection_id() << " upstream: lost connection with: " << remote_domain()
          << " due to " << ec;
  upstream_readable_ = false;
  upstream_writable_ = false;
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  if (downstream_.empty() && !shutdown_) {
    VLOG(2) << "Connection (client) " << connection_id() << " last data sent: shutting down";
    shutdown_ = true;
    downlink_->shutdown(ec);
  } else {
    WriteStream();
  }
}

void CliConnection::EncryptData(IoQueue* queue, std::shared_ptr<IOBuf> plaintext) {
  std::shared_ptr<IOBuf> cipherbuf;
  if (queue->empty()) {
    cipherbuf = IOBuf::create(SOCKET_DEBUF_SIZE);
    queue->push_back(cipherbuf);
  } else {
    cipherbuf = queue->back();
  }
  cipherbuf->reserve(0, plaintext->length() + (plaintext->length() / SS_FRAME_SIZE + 1) * 100);

  size_t plaintext_offset = 0;
  while (plaintext_offset < plaintext->length()) {
    size_t plaintext_size = std::min<int>(plaintext->length() - plaintext_offset, SS_FRAME_SIZE);
    encoder_->encrypt(plaintext->data() + plaintext_offset, plaintext_size, cipherbuf);
    plaintext_offset += plaintext_size;
  }
}

}  // namespace net::cli
