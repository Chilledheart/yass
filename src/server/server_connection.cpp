// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "server_connection.hpp"

#include <absl/base/attributes.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/base64.hpp"
#include "core/http_parser.hpp"
#include "core/rand_util.hpp"
#include "core/utils.hpp"

namespace server {

namespace {
std::vector<http2::adapter::Header> GenerateHeaders(
  std::vector<std::pair<std::string, std::string>> headers, int status = 0) {
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
  std::string result;
  auto user_pass = absl::StrCat(absl::GetFlag(FLAGS_username), ":",
                                absl::GetFlag(FLAGS_password));
  Base64Encode(user_pass, &result);
  return result;
}

} // namespace

const char ServerConnection::http_connect_reply_[] =
    "HTTP/1.1 200 Connection established\r\n\r\n";

bool DataFrameSource::Send(absl::string_view frame_header, size_t payload_length)  {
  std::string concatenated;
  if (payload_length) {
    DCHECK(!chunks_.empty());
    absl::string_view payload(
        reinterpret_cast<const char*>(chunks_.front()->data()), payload_length);
    concatenated = absl::StrCat(frame_header, payload);
  } else {
    concatenated = std::string(frame_header);
  }
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

  if (!payload_length) {
    return true;
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

ServerConnection::ServerConnection(asio::io_context& io_context,
                                   const std::string& remote_host_name,
                                   uint16_t remote_port,
                                   bool upstream_https_fallback,
                                   bool https_fallback,
                                   bool enable_upstream_tls,
                                   bool enable_tls,
                                   asio::ssl::context *upstream_ssl_ctx,
                                   asio::ssl::context *ssl_ctx)
    : Connection(io_context, remote_host_name, remote_port,
                 upstream_https_fallback, https_fallback,
                 enable_upstream_tls, enable_tls,
                 upstream_ssl_ctx, ssl_ctx),
      state_() {}

ServerConnection::~ServerConnection() {
  VLOG(1) << "Connection (server) " << connection_id() << " freed memory";
}

void ServerConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  closing_ = false;
  upstream_writable_ = false;
  downstream_readable_ = true;
  asio::error_code ec;
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);

  scoped_refptr<ServerConnection> self(this);
  if (enable_tls_) {
    ssl_socket_->Handshake([this, self](
      int result) {
        if (closed_ || closing_) {
          return;
        }
        if (result != net::OK) {
          SetState(state_error);
          OnDisconnect(asio::error::connection_refused);
          return;
        }
        Start();
    });
  } else {
    Start();
  }
}

void ServerConnection::close() {
  if (closing_) {
    return;
  }
  VLOG(1) << "Connection (server) " << connection_id()
          << " disconnected with client at stage: "
          << ServerConnection::state_to_str(CurrentState());
  asio::error_code ec;
  closing_ = true;

  if (adapter_) {
    if (data_frame_) {
      data_frame_->set_last_frame(true);
      adapter_->ResumeStream(stream_id_);
      SendIfNotProcessing();
      data_frame_ = nullptr;
      stream_id_ = 0;
    }
    adapter_->SubmitShutdownNotice();
    SendIfNotProcessing();
    WriteStreamInPipe();
  }
  closed_ = true;
  if (enable_tls_ && !shutdown_) {
    shutdown_ = true;
    ssl_socket_->Disconnect();
  } else {
    socket_.close(ec);
  }
  if (ec) {
    VLOG(1) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  auto cb = std::move(disconnect_cb_);
  disconnect_cb_ = nullptr;
  if (cb) {
    cb();
  }
}

void ServerConnection::Start() {
  bool http2 = absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2;
  http2 |= absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2_TLS;
  if (http2 && https_fallback_) {
    http2 = false;
  }
  if (http2) {
#ifdef HAVE_NGHTTP2
    adapter_ = http2::adapter::NgHttp2Adapter::CreateServerAdapter(*this);
#else
    http2::adapter::OgHttp2Adapter::Options options;
    options.perspective = http2::adapter::Perspective::kServer;
    adapter_ = http2::adapter::OgHttp2Adapter::Create(*this, options);
#endif
    padding_support_ = absl::GetFlag(FLAGS_padding_support);
    SetState(state_stream);

    // Send Upstream Settings (HTTP2 Only)
    std::vector<http2::adapter::Http2Setting> settings {
      { http2::adapter::Http2KnownSettingsId::HEADER_TABLE_SIZE,
        kSpdyMaxHeaderTableSize },
      { http2::adapter::Http2KnownSettingsId::MAX_CONCURRENT_STREAMS,
        kSpdyMaxConcurrentPushedStreams },
      { http2::adapter::Http2KnownSettingsId::INITIAL_WINDOW_SIZE,
        kSpdyStreamMaxRecvWindowSize },
      { http2::adapter::Http2KnownSettingsId::MAX_HEADER_LIST_SIZE,
        kSpdyMaxHeaderListSize },
      { http2::adapter::Http2KnownSettingsId::ENABLE_PUSH,
        kSpdyDisablePush },
    };
    adapter_->SubmitSettings(settings);
    SendIfNotProcessing();

    WriteUpstreamInPipe();
    OnUpstreamWriteFlush();
  } else if (https_fallback_) {
    // TODO should we support it?
    // padding_support_ = absl::GetFlag(FLAGS_padding_support);
    ReadHandshakeViaHttps();
  } else {
    encoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password),
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
      this, true);
    decoder_ = std::make_unique<cipher>("", absl::GetFlag(FLAGS_password),
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
      this);
    ReadHandshake();
  }
}

void ServerConnection::SendIfNotProcessing() {
  if (!processing_responses_) {
    processing_responses_ = true;
    adapter_->Send();
    processing_responses_ = false;
  }
}

//
// cipher_visitor_interface
//
bool ServerConnection::on_received_data(std::shared_ptr<IOBuf> buf) {
  MSAN_CHECK_MEM_IS_INITIALIZED(buf->data(), buf->length());
  if (state_ == state_stream) {
    upstream_.push_back(buf);
  } else if (state_ == state_handshake) {
    if (handshake_) {
      handshake_->reserve(0, buf->length());
      memcpy(handshake_->mutable_tail(), buf->data(), buf->length());
      handshake_->append(buf->length());
    } else {
      handshake_ = buf;
    }
  } else {
    return false;
  }
  return true;
}

void ServerConnection::on_protocol_error() {
  LOG(WARNING) << "Connection (server) " << connection_id()
    << " Protocol error";
  OnDisconnect(asio::error::connection_aborted);
}

//
// http2::adapter::Http2VisitorInterface
//

int64_t ServerConnection::OnReadyToSend(absl::string_view serialized) {
  MSAN_UNPOISON(serialized.data(), serialized.size());
  std::shared_ptr<IOBuf> buf =
    IOBuf::copyBuffer(serialized.data(), serialized.size());
  downstream_.push_back(buf);
  return serialized.size();
}

http2::adapter::Http2VisitorInterface::OnHeaderResult
ServerConnection::OnHeaderForStream(StreamId stream_id,
                                    absl::string_view key,
                                    absl::string_view value) {
  request_map_[key] = std::string(value);
  return http2::adapter::Http2VisitorInterface::HEADER_OK;
}

bool ServerConnection::OnEndHeadersForStream(
  http2::adapter::Http2StreamId stream_id) {

  if (request_map_[":method"] != "CONNECT") {
    VLOG(1) << "Connection (server) " << connection_id()
      << " Unexpected method: " << request_map_[":method"];
    return false;
  }
  auto auth = request_map_["proxy-authorization"];
  if (auth != "basic " + GetProxyAuthorizationIdentity()) {
    VLOG(1) << "Connection (server) " << connection_id()
      << " Unexpected auth token.";
    return false;
  }
  auto padding_support = request_map_.find("padding") != request_map_.end();
  asio::error_code ec;
  auto peer_endpoint = socket_.remote_endpoint(ec);
  if (padding_support_ && padding_support) {
    LOG(INFO) << "Connection (server) " << connection_id() << " for "
      << peer_endpoint << " Padding support enabled.";
  } else {
    VLOG(1) << "Connection (server) " << connection_id() << " for "
      << peer_endpoint << " Padding support disabled.";
    padding_support_ = false;
  }
  std::vector<std::string> host_and_port = absl::StrSplit(request_map_[":authority"], ":");
  if (host_and_port.size() != 2) {
    VLOG(1) << "Connection (server) " << connection_id()
      << " Unexpected authority: " << request_map_[":authority"];
    return false;
  }

  auto host = host_and_port[0];
  auto port = host_and_port[1];
  // FIXME remove stoi call
  request_ = ss::request(host, std::stoi(port));

  SetState(state_stream);
  OnConnect();
  return true;
}

bool ServerConnection::OnEndStream(StreamId stream_id) {
  if (stream_id == stream_id_) {
    data_frame_ = nullptr;
    stream_id_ = 0;
    OnDisconnect(asio::error::eof);
  }
  return true;
}

bool ServerConnection::OnCloseStream(StreamId stream_id,
                                     http2::adapter::Http2ErrorCode error_code) {
  if (stream_id == stream_id_) {
    data_frame_ = nullptr;
    stream_id_ = 0;
  }
  return true;
}

void ServerConnection::OnConnectionError(ConnectionError /*error*/) {
  OnDisconnect(asio::error::connection_aborted);
}

bool ServerConnection::OnFrameHeader(StreamId stream_id,
                                     size_t /*length*/,
                                     uint8_t /*type*/,
                                     uint8_t /*flags*/) {
  return true;
}

bool ServerConnection::OnBeginHeadersForStream(StreamId stream_id) {
  if (!stream_id_) {
    stream_id_ = stream_id;
  }
  if (stream_id) {
    DCHECK_EQ(stream_id, stream_id_) << "Server only support one stream";
  }
  return true;
}

bool ServerConnection::OnBeginDataForStream(StreamId stream_id,
                                            size_t payload_length) {
  return true;
}

bool ServerConnection::OnDataForStream(StreamId stream_id,
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
      upstream_.push_back(buf);
      ++num_padding_recv_;
    }
    // Deal with in_middle_buf outside paddings
    if (num_padding_recv_ >= kFirstPaddings && !padding_in_middle_buf_->empty()) {
      upstream_.push_back(std::move(padding_in_middle_buf_));
    }
    return true;
  }

  std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(data.data(), data.size());
  upstream_.push_back(buf);
  adapter_->MarkDataConsumedForStream(stream_id, data.size());
  return true;
}

bool ServerConnection::OnDataPaddingLength(StreamId stream_id,
                                           size_t padding_length) {
  adapter_->MarkDataConsumedForStream(stream_id, padding_length);
  return true;
}

void ServerConnection::OnRstStream(StreamId stream_id,
                                   http2::adapter::Http2ErrorCode error_code) {
  OnDisconnect(asio::error::connection_reset);
}

bool ServerConnection::OnGoAway(StreamId last_accepted_stream_id,
                                http2::adapter::Http2ErrorCode error_code,
                                absl::string_view opaque_data) {
  OnDisconnect(asio::error::connection_reset);
  return true;
}

int ServerConnection::OnBeforeFrameSent(uint8_t frame_type,
                                        StreamId stream_id,
                                        size_t length,
                                        uint8_t flags) {
  return 0;
}

int ServerConnection::OnFrameSent(uint8_t frame_type,
                                  StreamId stream_id,
                                  size_t length,
                                  uint8_t flags,
                                  uint32_t error_code) {
  return 0;
}

bool ServerConnection::OnInvalidFrame(StreamId stream_id,
                                      InvalidFrameError error) {
  return true;
}

bool ServerConnection::OnMetadataForStream(StreamId stream_id,
                                           absl::string_view metadata) {
  return true;
}

bool ServerConnection::OnMetadataEndForStream(StreamId stream_id) {
  return true;
}

void ServerConnection::ReadHandshake() {
  scoped_refptr<ServerConnection> self(this);

  s_async_read_some_([this, self](asio::error_code ec) {
      if (closed_ || closing_) {
        return;
      }
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        ProcessReceivedData(nullptr, ec, 0);
        return;
      }
      std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(SOCKET_DEBUF_SIZE);
      size_t bytes_transferred;
      do {
        bytes_transferred = s_read_some_(cipherbuf, ec);
        if (ec == asio::error::interrupted) {
          continue;
        }
      } while(false);
      if (ec == asio::error::try_again || ec == asio::error::would_block) {
        DCHECK_EQ(bytes_transferred, 0u);
        ReadHandshake();
        return;
      }
      if (ec) {
        OnDisconnect(ec);
        return;
      }
      cipherbuf->append(bytes_transferred);
      decoder_->process_bytes(cipherbuf);
      if (!handshake_) {
        ReadHandshake();
        return;
      }
      auto buf = handshake_;

      DumpHex("HANDSHAKE->", buf.get());

      ss::request_parser::result_type result;
      std::tie(result, std::ignore) = request_parser_.parse(
          request_, buf->data(), buf->data() + bytes_transferred);

      if (result == ss::request_parser::good) {
        buf->trimStart(request_.length());
        buf->retreat(request_.length());
        DCHECK_LE(request_.length(), bytes_transferred);
        ProcessReceivedData(buf, ec, buf->length());
      } else {
        // FIXME better error code?
        ec = asio::error::connection_refused;
        OnDisconnect(ec);
      }
  });
}

void ServerConnection::ReadHandshakeViaHttps() {
  scoped_refptr<ServerConnection> self(this);

  if (DoPeek()) {
    OnReadHandshakeViaHttps();
    return;
  }

  s_async_read_some_([this, self](asio::error_code ec) {
    if (closed_ || closing_) {
      return;
    }
    if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
      return;
    }
    if (ec) {
      ProcessReceivedData(nullptr, ec, 0);
      return;
    }
    OnReadHandshakeViaHttps();
  });
}

void ServerConnection::OnReadHandshakeViaHttps() {
  asio::error_code ec;
  size_t bytes_transferred;
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_DEBUF_SIZE);
  do {
    bytes_transferred = s_read_some_(buf, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  if (ec == asio::error::try_again || ec == asio::error::would_block) {
    DCHECK_EQ(bytes_transferred, 0u);
    ReadHandshakeViaHttps();
    return;
  }
  if (ec) {
    OnDisconnect(ec);
    return;
  }
  buf->append(bytes_transferred);

  DumpHex("HANDSHAKE->", buf.get());

  HttpRequestParser parser;

  bool ok;
  int nparsed = parser.Parse(buf, &ok);
  if (nparsed) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " http: "
            << std::string(reinterpret_cast<const char*>(buf->data()), nparsed);
  }

  if (ok) {
    buf->trimStart(nparsed);
    buf->retreat(nparsed);

    http_host_ = parser.host();
    http_port_ = parser.port();
    http_is_connect_ = parser.is_connect();

    request_ = {http_host_, http_port_};

    if (!http_is_connect_) {
      std::string header;
      parser.ReforgeHttpRequest(&header);
      buf->reserve(header.size(), 0);
      buf->prepend(header.size());
      memcpy(buf->mutable_data(), header.c_str(), header.size());
      VLOG(3) << "Connection (server) " << connection_id()
              << " Host: " << http_host_ << " PORT: " << http_port_;
    } else {
      VLOG(3) << "Connection (server) " << connection_id()
              << " CONNECT: " << http_host_ << " PORT: " << http_port_;
    }
    ProcessReceivedData(buf, ec, buf->length());
  } else {
    // FIXME better error code?
    ec = asio::error::connection_refused;
    OnDisconnect(ec);
  }
}

void ServerConnection::ReadStream() {
  scoped_refptr<ServerConnection> self(this);
  DCHECK_EQ(downstream_read_inprogress_, false);
  if (downstream_read_inprogress_) {
    return;
  }

  if (closed_ || closing_) {
    return;
  }

  downstream_read_inprogress_ = true;
  s_async_read_some_([this, self](asio::error_code ec) {
      downstream_read_inprogress_ = false;
      if (closed_ || closing_) {
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

void ServerConnection::WriteStream() {
  DCHECK(!write_inprogress_);
  if (write_inprogress_) {
    return;
  }
  scoped_refptr<ServerConnection> self(this);
  write_inprogress_ = true;
  s_async_write_some_([this, self](asio::error_code ec) {
      write_inprogress_ = false;
      if (closed_ || closing_) {
        return;
      }
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        ProcessSentData(ec, 0);
        return;
      }
      WriteStreamInPipe();
  });
}

void ServerConnection::WriteStreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0U, wbytes_transferred = 0U;
  bool try_again = false;

  /* recursively send the remainings */
  while (true) {
    auto buf = GetNextDownstreamBuf(ec, &bytes_transferred);
    size_t read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      try_again = true;
    } else if (ec) {
      /* not downstream error */
      ec = asio::error_code();
      break;
    }
    if (!read) {
      break;
    }
    if (closed_ || closing_) {
      break;
    }
    ec = asio::error_code();
    size_t written;
    do {
      written = s_write_some_(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->trimStart(written);
    bytes_downstream_passed_without_yield_ += written;
    wbytes_transferred += written;
    // continue to resume
    if (buf->empty()) {
      downstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      break;
    }
    if (!buf->empty()) {
      ec = asio::error::try_again;
      break;
    }
    if (GetMonotonicTime() > yield_downstream_after_time_ ||
        bytes_downstream_passed_without_yield_ > kYieldAfterBytesRead) {
      bytes_downstream_passed_without_yield_ = 0U;
      yield_downstream_after_time_ = GetMonotonicTime() + kYieldAfterDurationMilliseconds * 1000 * 1000;
      if (downstream_.empty()) {
        try_again = true;
      } else {
        ec = asio::error::try_again;
      }
      break;
    }
  }
  if (try_again) {
    if (channel_ && channel_->connected() && !channel_->read_inprogress()) {
      scoped_refptr<ServerConnection> self(this);
      channel_->start_read([self]() {});
    }
  }
  if (ec == asio::error::try_again || ec == asio::error::would_block) {
    OnDownstreamWriteFlush();
    if (!wbytes_transferred) {
      return;
    }
    ec = asio::error_code();
  }
  if (!bytes_transferred && !ec && !try_again) {
    OnStreamWrite();
    return;
  }
  ProcessSentData(ec, wbytes_transferred);
}

std::shared_ptr<IOBuf> ServerConnection::GetNextDownstreamBuf(asio::error_code &ec,
                                                              size_t* bytes_transferred) {
  if (!downstream_.empty()) {
    DCHECK(!downstream_.front()->empty());
    ec = asio::error_code();
    return downstream_.front();
  }
  if (pending_downstream_read_error_) {
    ec = std::move(pending_downstream_read_error_);
    return nullptr;
  }
  if (!channel_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  if (channel_->eof()) {
    ec = asio::error::eof;
    return nullptr;
  }

  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
  size_t read;
  do {
    ec = asio::error_code();
    read = channel_->read_some(buf, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    // handled in channel_->read_some func
    // disconnected(ec);
    goto out;
  }
  if (read) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " upstream: received reply (pipe): " << read << " bytes."
            << " done: " << channel_->rbytes_transferred() << " bytes.";
  } else {
    goto out;
  }
  *bytes_transferred += read;

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
  } else if (https_fallback_) {
    downstream_.push_back(buf);
  } else {
    EncryptData(&downstream_, buf);
  }

out:
  if (data_frame_ && *bytes_transferred) {
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter_->ResumeStream(stream_id_);
    SendIfNotProcessing();
  }
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

void ServerConnection::WriteUpstreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0U, wbytes_transferred = 0U;
  bool try_again = false;

  if (channel_ && channel_->write_inprogress()) {
    return;
  }

  /* recursively send the remainings */
  while (true) {
    size_t read;
    std::shared_ptr<IOBuf> buf = GetNextUpstreamBuf(ec, &bytes_transferred);
    read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      try_again = true;
    } else if (ec) {
      /* handled in getter */
      return;
    }
    if (!read) {
      break;
    }
    if (!channel_ || !channel_->connected() || channel_->eof()) {
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
    } while(false);
    buf->trimStart(written);
    wbytes_transferred += written;
    VLOG(2) << "Connection (server) " << connection_id()
            << " upstream: sent request (pipe): " << written << " bytes"
            << " done: " << channel_->wbytes_transferred() << " bytes."
            << " ec: " << ec;
    // continue to resume
    if (buf->empty()) {
      upstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      VLOG(1) << "Connection (server) " << connection_id()
              << " disabling reading";
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
    if (!buf->empty()) {
      ec = asio::error::try_again;
      break;
    }
    if (GetMonotonicTime() > yield_upstream_after_time_ ||
        bytes_upstream_passed_without_yield_ > kYieldAfterBytesRead) {
      bytes_upstream_passed_without_yield_ = 0U;
      yield_upstream_after_time_ = GetMonotonicTime() + kYieldAfterDurationMilliseconds;
      if (upstream_.empty()) {
        try_again = true;
      } else {
        ec = asio::error::try_again;
      }
      break;
    }
  }
  if (try_again) {
    if (!downstream_read_inprogress_) {
      ReadStream();
    }
  }
  if (ec == asio::error::try_again || ec == asio::error::would_block) {
    OnUpstreamWriteFlush();
    return;
  }
}

std::shared_ptr<IOBuf> ServerConnection::GetNextUpstreamBuf(asio::error_code &ec,
                                                            size_t* bytes_transferred) {
  if (!upstream_.empty()) {
    DCHECK(!upstream_.front()->empty());
    ec = asio::error_code();
    return upstream_.front();
  }
  if (pending_upstream_read_error_) {
    ec = std::move(pending_upstream_read_error_);
    return nullptr;
  }

try_again:
  // RstStream might be sent in ProcessBytes
  if (closed_ || closing_) {
    ec = asio::error::eof;
    return nullptr;
  }
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_DEBUF_SIZE);
  size_t read;
  do {
    read = s_read_some_(buf, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    /* safe to return, socket will handle this error later */
    ProcessReceivedData(nullptr, ec, read);
    goto out;
  }
  *bytes_transferred += read;
  rbytes_transferred_ += read;
  if (read) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " received data (pipe): " << read << " bytes."
            << " done: " << rbytes_transferred_ << " bytes.";
  } else {
    goto out;
  }

  if (adapter_) {
    absl::string_view remaining_buffer(
        reinterpret_cast<const char*>(buf->data()), buf->length());
    while (!remaining_buffer.empty()) {
      int result = adapter_->ProcessBytes(remaining_buffer);
      if (result < 0) {
        ec = asio::error::connection_refused;
        OnDisconnect(asio::error::connection_refused);
        return nullptr;
      }
      remaining_buffer = remaining_buffer.substr(result);
    }
    // not data frame, try again
    if (upstream_.empty()) {
      goto try_again;
    }
  } else if (https_fallback_) {
    upstream_.push_back(buf);
  } else {
    decoder_->process_bytes(buf);
  }

out:
  if (adapter_ && adapter_->want_write()) {
    // Send Control Streams
    SendIfNotProcessing();
    WriteStreamInPipe();
  }
  if (upstream_.empty()) {
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

void ServerConnection::ProcessReceivedData(std::shared_ptr<IOBuf> buf,
                                           asio::error_code ec,
                                           size_t bytes_transferred) {
  rbytes_transferred_ += bytes_transferred;
  VLOG(2) << "Connection (server) " << connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " done: " << rbytes_transferred_ << " bytes."
          << " ec: " << ec;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (CurrentState()) {
      case state_handshake:
        SetState(state_stream);
        OnConnect();
        DCHECK_EQ(buf->length(), bytes_transferred);
        ABSL_FALLTHROUGH_INTENDED;
        /* fall through */
      case state_stream:
        DCHECK_EQ(bytes_transferred, buf->length());
        if (bytes_transferred) {
          OnStreamRead(buf);
          return;
        }
        WriteUpstreamInPipe();
        OnUpstreamWriteFlush();
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (server) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    };
  }
  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
}

void ServerConnection::ProcessSentData(asio::error_code ec,
                                       size_t bytes_transferred) {
  wbytes_transferred_ += bytes_transferred;

  VLOG(2) << "Connection (server) " << connection_id()
          << " sent data: " << bytes_transferred << " bytes."
          << " done: " << wbytes_transferred_ << " bytes."
          << " ec: " << ec;

  if (!ec) {
    switch (CurrentState()) {
      case state_stream:
        if (bytes_transferred) {
          OnStreamWrite();
        }
        break;
      case state_handshake:
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (server) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    }
  }

  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
}

void ServerConnection::OnConnect() {
  scoped_refptr<ServerConnection> self(this);
  LOG(INFO) << "Connection (server) " << connection_id()
            << " connect " << remote_domain();
  std::string host_name;
  uint16_t port = request_.port();
  if (request_.address_type() == ss::domain) {
    host_name = request_.domain_name();
  } else {
    host_name = request_.endpoint().address().to_string();
  }
  channel_ = std::make_unique<stream>(*io_context_,
                                      host_name, port,
                                      this, upstream_https_fallback_,
                                      enable_upstream_tls_, upstream_ssl_ctx_);
  channel_->connect([self]{});
  if (adapter_) {
    // stream is ready
    std::unique_ptr<DataFrameSource> data_frame =
      std::make_unique<DataFrameSource>(this, stream_id_);
    data_frame_ = data_frame.get();
    std::vector<std::pair<std::string, std::string>> headers;
    // Send "Padding" header
    // originated from forwardproxy.go;func ServeHTTP
    if (padding_support_) {
      std::string padding(RandInt(30, 64), '~');
      uint64_t bits = RandUint64();
      for (int i = 0; i < 16; ++i) {
        padding[i] = "!#$()+<>?@[]^`{}"[bits & 15];
        bits = bits >> 4;
      }
      headers.emplace_back("padding", padding);
    }
    int submit_result = adapter_->SubmitResponse(stream_id_,
                                                 GenerateHeaders(headers, 200),
                                                 std::move(data_frame));
    SendIfNotProcessing();
    if (submit_result != 0) {
      OnDisconnect(asio::error::connection_aborted);
    }
  } else if (https_fallback_ && http_is_connect_) {
    std::shared_ptr<IOBuf> buf = IOBuf::copyBuffer(
        http_connect_reply_, sizeof(http_connect_reply_) - 1);
    OnDownstreamWrite(buf);
  }
}

void ServerConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  OnUpstreamWrite(buf);
}

void ServerConnection::OnStreamWrite() {
  if (blocked_stream_) {
    adapter_->ResumeStream(blocked_stream_);
    SendIfNotProcessing();
  }
  /* shutdown the socket if upstream is eof and all remaining data sent */
  bool nodata = !data_frame_ || !data_frame_->SelectPayloadLength(1).first;
  if (channel_ && channel_->eof() && nodata && downstream_.empty() && !shutdown_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " last data sent: shutting down";
    shutdown_ = true;
    if (data_frame_) {
      data_frame_->set_last_frame(true);
      adapter_->ResumeStream(stream_id_);
      SendIfNotProcessing();
      data_frame_ = nullptr;
      stream_id_ = 0;
      WriteStreamInPipe();
      return;
    }
    scoped_refptr<ServerConnection> self(this);
    s_async_shutdown_([this, self](asio::error_code ec) {
      if (closed_ || closing_) {
        return;
      }
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        VLOG(1) << "Connection (server) " << connection_id()
                << " erorr occured in shutdown: " << ec;
        OnDisconnect(ec);
        return;
      }
    });
    return;
  }

  OnDownstreamWriteFlush();
}

void ServerConnection::OnDisconnect(asio::error_code ec) {
  if (closing_) {
    return;
  }
#ifdef WIN32
  if (ec.value() == WSAESHUTDOWN) {
    ec = asio::error_code();
  }
#else
  if (ec.value() == asio::error::operation_aborted) {
    ec = asio::error_code();
  }
#endif
  LOG(INFO) << "Connection (server) " << connection_id()
            << " closed: " << ec;
  close();
}

void ServerConnection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void ServerConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    downstream_.push_back(buf);
  }

  if (!downstream_.empty() && !write_inprogress_) {
    WriteStream();
  }
}

void ServerConnection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void ServerConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    scoped_refptr<ServerConnection> self(this);
    channel_->start_write([self](){});
  }
}

void ServerConnection::connected() {
  scoped_refptr<ServerConnection> self(this);
  VLOG(1) << "Connection (server) " << connection_id()
          << " remote: established upstream connection with: "
          << remote_domain();
  upstream_readable_ = true;
  upstream_writable_ = true;

  yield_upstream_after_time_ = yield_downstream_after_time_ =
    GetMonotonicTime() + kYieldAfterDurationMilliseconds * 1000 * 1000;

  WriteStreamInPipe();
  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();
}

void ServerConnection::received() {
  scoped_refptr<ServerConnection> self(this);

  WriteStreamInPipe();
  OnDownstreamWriteFlush();
}

void ServerConnection::sent() {
  scoped_refptr<ServerConnection> self(this);

  upstream_writable_ = true;

  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();
}

void ServerConnection::disconnected(asio::error_code ec) {
  scoped_refptr<ServerConnection> self(this);
  VLOG(1) << "Connection (server) " << connection_id()
          << " upstream: lost connection with: " << remote_domain()
          << " due to " << ec;
  upstream_readable_ = false;
  upstream_writable_ = false;
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  bool nodata = !data_frame_ || !data_frame_->SelectPayloadLength(1).first;
  if (nodata && downstream_.empty() && !shutdown_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " upstream: last data sent: shutting down";
    shutdown_ = true;
    if (data_frame_) {
      data_frame_->set_last_frame(true);
      adapter_->ResumeStream(stream_id_);
      SendIfNotProcessing();
      data_frame_ = nullptr;
      stream_id_ = 0;
      WriteStreamInPipe();
      return;
    }
    scoped_refptr<ServerConnection> self(this);
    s_async_shutdown_([this, self](asio::error_code ec) {
      if (closed_ || closing_) {
        return;
      }
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        VLOG(1) << "Connection (server) " << connection_id()
                << " erorr occured in shutdown: " << ec;
        OnDisconnect(ec);
        return;
      }
    });
  } else {
    WriteStreamInPipe();
  }
}

void ServerConnection::EncryptData(IoQueue* queue,
                                   std::shared_ptr<IOBuf> plaintext) {
  size_t plaintext_offset = 0;
  while (plaintext_offset < plaintext->length()) {
    size_t plaintext_size = std::min<int>(plaintext->length() - plaintext_offset,
                                          SS_FRAME_SIZE);
    std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(plaintext->length() + 100);

    encoder_->encrypt(plaintext->data() + plaintext_offset, plaintext_size, &cipherbuf);
    MSAN_CHECK_MEM_IS_INITIALIZED(cipherbuf->data(), cipherbuf->length());
    queue->push_back(cipherbuf);
    plaintext_offset += plaintext_size;
  }
}

}  // namespace server
