// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "ss_connection.hpp"

#include <absl/base/attributes.h>
#include <absl/strings/str_split.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/base64.hpp"
#include "core/http_parser.hpp"
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

} // namespace

// 32K / 4k = 8
#define MAX_DOWNSTREAM_DEPS 8
#define MAX_UPSTREAM_DEPS 8

namespace ss {

const char SsConnection::http_connect_reply_[] =
    "HTTP/1.1 200 Connection established\r\n\r\n";

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

SsConnection::SsConnection(asio::io_context& io_context,
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
      state_(),
      resolver_(*io_context_) {}

SsConnection::~SsConnection() {
  VLOG(2) << "Connection (server) " << connection_id() << " freed memory";
}

void SsConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  upstream_writable_ = false;
  downstream_readable_ = true;
  asio::error_code ec;
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);

  scoped_refptr<SsConnection> self(this);
  if (enable_tls_) {
    ssl_socket_.async_handshake(asio::ssl::stream_base::server,
                                [self](asio::error_code ec) {
      if (ec) {
        self->SetState(state_error);
        self->OnDisconnect(ec);
        return;
      }
      self->Start();
    });
  } else {
    Start();
  }
}

void SsConnection::close() {
  if (closed_) {
    return;
  }
  size_t bytes = 0;
  for (auto buf : downstream_)
    bytes += buf->length();
  VLOG(2) << "Connection (server) " << connection_id()
          << " disconnected with client at stage: "
          << SsConnection::state_to_str(CurrentState())
          << " and remaining: " << bytes << " bytes.";
  asio::error_code ec;
  closed_ = true;
  if (enable_tls_) {
    // FIXME use async_shutdown correctly
    ssl_socket_.shutdown(ec);
  }
  socket_.close(ec);
  if (ec) {
    VLOG(2) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  resolver_.cancel();
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void SsConnection::Start() {
  bool http2 = absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2;
  http2 |= absl::GetFlag(FLAGS_cipher_method) == CRYPTO_HTTP2_TLS;
  if (http2 && https_fallback_) {
    http2 = false;
  }
  if (http2) {
    http2::adapter::OgHttp2Adapter::Options options;
    options.perspective = http2::adapter::Perspective::kServer;
    adapter_ = http2::adapter::OgHttp2Adapter::Create(*this, options);
    padding_support_ = absl::GetFlag(FLAGS_padding_support);
    SetState(state_stream);
    ReadStream();
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

void SsConnection::SendIfNotProcessing() {
  if (!processing_responses_) {
    processing_responses_ = true;
    adapter_->Send();
    processing_responses_ = false;
  }
}

//
// cipher_visitor_interface
//
bool SsConnection::on_received_data(std::shared_ptr<IOBuf> buf) {
  MSAN_CHECK_MEM_IS_INITIALIZED(buf->data(), buf->length());
  rbytes_transferred_ += buf->length();
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

void SsConnection::on_protocol_error() {
  LOG(WARNING) << "Connection (server) " << connection_id()
    << " Protocol error";
  OnDisconnect(asio::error::connection_aborted);
}

//
// http2::adapter::Http2VisitorInterface
//

int64_t SsConnection::OnReadyToSend(absl::string_view serialized) {
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    return kSendBlocked;
  }

  std::shared_ptr<IOBuf> buf =
    IOBuf::copyBuffer(serialized.data(), serialized.size());
  downstream_.push_back(buf);
  return serialized.size();
}

http2::adapter::Http2VisitorInterface::OnHeaderResult
SsConnection::OnHeaderForStream(StreamId stream_id,
                                absl::string_view key,
                                absl::string_view value) {
  request_map_[key] = std::string(value);
  return http2::adapter::Http2VisitorInterface::HEADER_OK;
}

bool SsConnection::OnEndHeadersForStream(
  http2::adapter::Http2StreamId stream_id) {

  if (request_map_[":method"] != "CONNECT") {
    VLOG(2) << "Connection (server) " << connection_id()
      << " Unexpected method: " << request_map_[":method"];
    return false;
  }
  auto auth = request_map_["proxy-authorization"];
  if (auth != "basic " + GetProxyAuthorizationIdentity()) {
    VLOG(2) << "Connection (server) " << connection_id()
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
    VLOG(2) << "Connection (server) " << connection_id() << " for "
      << peer_endpoint << " Padding support disabled.";
    padding_support_ = false;
  }
  std::vector<std::string> host_and_port = absl::StrSplit(request_map_[":authority"], ":");
  if (host_and_port.size() != 2) {
    VLOG(2) << "Connection (server) " << connection_id()
      << " Unexpected authority: " << request_map_[":authority"];
    return false;
  }

  auto host = host_and_port[0];
  auto port = host_and_port[1];
  // FIXME remove stoi call
  request_ = request(host, std::stoi(port));

  ResolveDns(nullptr);
  return true;
}

bool SsConnection::OnEndStream(StreamId stream_id) {
  return true;
}

bool SsConnection::OnCloseStream(StreamId stream_id,
                                 http2::adapter::Http2ErrorCode error_code) {
  OnDisconnect(asio::error_code());
  return true;
}

void SsConnection::OnConnectionError(ConnectionError /*error*/) {
  OnDisconnect(asio::error::connection_aborted);
}

bool SsConnection::OnFrameHeader(StreamId stream_id,
                                 size_t /*length*/,
                                 uint8_t /*type*/,
                                 uint8_t /*flags*/) {
  if (!stream_id_) {
    stream_id_ = stream_id;
  }
  if (stream_id) {
    DCHECK_EQ(stream_id, stream_id_) << "Server only support one stream";
  }
  return true;
}

bool SsConnection::OnBeginHeadersForStream(StreamId stream_id) {
  return true;
}

bool SsConnection::OnBeginDataForStream(StreamId stream_id,
                                        size_t payload_length) {
  return true;
}

bool SsConnection::OnDataForStream(StreamId stream_id,
                                   absl::string_view data) {
  rbytes_transferred_ += data.size();

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

bool SsConnection::OnDataPaddingLength(StreamId stream_id,
                                       size_t padding_length) {
  adapter_->MarkDataConsumedForStream(stream_id, padding_length);
  return true;
}

bool SsConnection::OnGoAway(StreamId last_accepted_stream_id,
                            http2::adapter::Http2ErrorCode error_code,
                            absl::string_view opaque_data) {
  return true;
}

int SsConnection::OnBeforeFrameSent(uint8_t frame_type,
                                    StreamId stream_id,
                                    size_t length,
                                    uint8_t flags) {
  return 0;
}

int SsConnection::OnFrameSent(uint8_t frame_type,
                              StreamId stream_id,
                              size_t length,
                              uint8_t flags,
                              uint32_t error_code) {
  return 0;
}

bool SsConnection::OnInvalidFrame(StreamId stream_id,
                                  InvalidFrameError error) {
  return true;
}

bool SsConnection::OnMetadataForStream(StreamId stream_id,
                                       absl::string_view metadata) {
  return true;
}

bool SsConnection::OnMetadataEndForStream(StreamId stream_id) {
  return true;
}

void SsConnection::ReadHandshake() {
  scoped_refptr<SsConnection> self(this);

  s_async_read_some_([self](asio::error_code ec, size_t bytes_transferred) {
        std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->s_read_some_(cipherbuf, ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadHandshake();
          return;
        }
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        cipherbuf->append(bytes_transferred);
        self->decoder_->process_bytes(cipherbuf);
        if (!self->handshake_) {
          self->ReadHandshake();
          return;
        }
        auto buf = self->handshake_;

        DumpHex("HANDSHAKE->", buf.get());

        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, buf->data(), buf->data() + bytes_transferred);

        if (result == request_parser::good) {
          buf->trimStart(self->request_.length());
          buf->retreat(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          self->ProcessReceivedData(buf, ec, buf->length());
        } else {
          // FIXME better error code?
          ec = asio::error::connection_refused;
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ReadHandshakeViaHttps() {
  scoped_refptr<SsConnection> self(this);

  s_async_read_some_([this, self](asio::error_code ec, size_t bytes_transferred) {
        std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->s_read_some_(buf, ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadHandshakeViaHttps();
          return;
        }
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        buf->append(bytes_transferred);

        DumpHex("HANDSHAKE->", buf.get());

        HttpRequestParser parser;

        bool ok;
        int nparsed = parser.Parse(buf, &ok);
        if (nparsed) {
          VLOG(4) << "Connection (server) " << connection_id()
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
            VLOG(4) << "Connection (server) " << connection_id()
                    << " Host: " << http_host_ << " PORT: " << http_port_;
          } else {
            VLOG(4) << "Connection (server) " << connection_id()
                    << " CONNECT: " << http_host_ << " PORT: " << http_port_;
          }
          ProcessReceivedData(buf, ec, buf->length());
        } else {
          // FIXME better error code?
          ec = asio::error::connection_refused;
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ResolveDns(std::shared_ptr<IOBuf> buf) {
  scoped_refptr<SsConnection> self(this);
  resolver_.async_resolve(
      self->request_.domain_name(), std::to_string(self->request_.port()),
      [self, buf](asio::error_code ec,
                  asio::ip::tcp::resolver::results_type results) {
        // Get a list of endpoints corresponding to the SOCKS 5 domain name.
        if (!ec) {
          self->remote_endpoint_ = results->endpoint();
          VLOG(3) << "Connection (server) " << self->connection_id()
                  << " resolved address: " << self->request_.domain_name()
                  << " to: " << self->remote_endpoint_;
          self->SetState(state_stream);
          self->OnConnect();
          if (buf) {
            self->ProcessReceivedData(buf, ec, buf->length());
          }
        } else {
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ReadStream() {
  scoped_refptr<SsConnection> self(this);
  downstream_read_inprogress_ = true;

  s_async_read_some_([self](asio::error_code ec,
                            std::size_t bytes_transferred) {
        self->downstream_read_inprogress_ = false;
        if (ec) {
          self->ProcessReceivedData(nullptr, ec, bytes_transferred);
          return;
        }
        if (!self->downstream_readable_) {
          return;
        }
        std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->s_read_some_(buf, ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadStream();
          return;
        }
        buf->append(bytes_transferred);
        if (self->adapter_) {
          absl::string_view remaining_buffer(
              reinterpret_cast<const char*>(buf->data()), buf->length());
          while (!remaining_buffer.empty()) {
            int result = self->adapter_->ProcessBytes(remaining_buffer);
            if (result < 0) {
              self->OnDisconnect(asio::error::connection_refused);
              return;
            }
            remaining_buffer = remaining_buffer.substr(result);
          }
          // Sent Control Streams
          self->SendIfNotProcessing();
          self->OnDownstreamWriteFlush();
        } else if (self->https_fallback_) {
          self->rbytes_transferred_ += buf->length();
          self->upstream_.push_back(buf);
        } else {
          self->decoder_->process_bytes(buf);
        }
        self->OnUpstreamWriteFlush();
        if (self->downstream_readable_) {
          self->ReadStream();
        }
      });
}

void SsConnection::WriteStream() {
  if (write_inprogress_) {
    return;
  }
  DCHECK(!write_inprogress_);
  scoped_refptr<SsConnection> self(this);
  write_inprogress_ = true;
  s_async_write_some_([self](asio::error_code ec,
                             size_t /*bytes_transferred*/) {
        self->write_inprogress_ = false;
        if (ec) {
          self->ProcessSentData(ec, 0);
          return;
        }
        self->WriteStreamInPipe();
      });
}

void SsConnection::WriteStreamInPipe() {
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
      written = s_write_some_(buf, ec);
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

std::shared_ptr<IOBuf> SsConnection::GetNextDownstreamBuf(asio::error_code &ec) {
  if (!downstream_.empty()) {
    ec = asio::error_code();
    return downstream_.front();
  }
  if (!upstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  size_t bytes_transferred = 0U;

repeat_fetch:
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
  if (read) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: received reply (pipe): " << read << " bytes.";
  } else {
    goto out;
  }
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
  } else if (https_fallback_) {
    downstream_.push_back(buf);
  } else {
    downstream_.push_back(EncryptData(buf));
  }

out:
  if (adapter_ && bytes_transferred) {
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter_->ResumeStream(stream_id_);
    SendIfNotProcessing();
  }
  if (downstream_.empty()) {
    ec = asio::error::try_again;
    return nullptr;
  }
  return downstream_.front();
}

void SsConnection::WriteUpstreamInPipe() {
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
      /* safe to return, channel will handle this error */
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
    VLOG(3) << "Connection (server) " << connection_id()
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

std::shared_ptr<IOBuf> SsConnection::GetNextUpstreamBuf(asio::error_code &ec) {
  if (!upstream_.empty()) {
    ec = asio::error_code();
    return upstream_.front();
  }
  if (!downstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  std::shared_ptr<IOBuf> buf = IOBuf::create(SOCKET_BUF_SIZE);
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
    return nullptr;
  }
  if (read) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " received data (pipe): " << read << " bytes.";
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
        OnDisconnect(asio::error::connection_refused);
        return nullptr;
      }
      remaining_buffer = remaining_buffer.substr(result);
    }
    // Sent Control Streams
    SendIfNotProcessing();
    OnDownstreamWriteFlush();
  } else if (https_fallback_) {
    rbytes_transferred_ += buf->length();
    upstream_.push_back(buf);
  } else {
    decoder_->process_bytes(buf);
  }
  if (upstream_.empty()) {
    ec = asio::error::try_again;
    return nullptr;
  }
  return upstream_.front();
}

void SsConnection::ProcessReceivedData(std::shared_ptr<IOBuf> buf,
                                       asio::error_code ec,
                                       size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " ec: " << ec;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (CurrentState()) {
      case state_handshake:
        if (request_.address_type() == domain) {
          ResolveDns(buf);
          return;
        }
        remote_endpoint_ = request_.endpoint();
        SetState(state_stream);
        OnConnect();
        DCHECK_EQ(buf->length(), bytes_transferred);
        ABSL_FALLTHROUGH_INTENDED;
        /* fall through */
      case state_stream:
        DCHECK_EQ(bytes_transferred, buf->length());
        if (bytes_transferred) {
          OnStreamRead(buf);
        }
        if (downstream_readable_) {
          ReadStream();  // continously read
        }
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

void SsConnection::ProcessSentData(asio::error_code ec,
                                   size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " sent data: " << bytes_transferred << " bytes"
          << " ec: " << ec << " and data to write: " << downstream_.size();

  wbytes_transferred_ += bytes_transferred;

  if (!ec) {
    switch (CurrentState()) {
      case state_stream:
        OnStreamWrite();
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
};

void SsConnection::OnConnect() {
  LOG(INFO) << "Connection (server) " << connection_id()
            << " to " << remote_domain();
  channel_ = std::make_unique<stream>(*io_context_, remote_endpoint_,
                                      this, upstream_https_fallback_,
                                      enable_upstream_tls_, upstream_ssl_ctx_);
  channel_->connect();
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

void SsConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  // queue limit to downstream read
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " disabling reading";
    DisableStreamRead();
  }

  OnUpstreamWrite(buf);
}

void SsConnection::OnStreamWrite() {
  if (blocked_stream_) {
    adapter_->ResumeStream(blocked_stream_);
    SendIfNotProcessing();
  }
  OnDownstreamWriteFlush();

  /* shutdown the socket if upstream is eof and all remaining data sent */
  bool nodata = !data_frame_ || !data_frame_->SelectPayloadLength(1).first;
  if (channel_ && channel_->eof() && nodata && downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " last data sent: shutting down";
    asio::error_code ec;
    s_shutdown_(ec);
    return;
  }

  /* disable queue limit to re-enable upstream read */
  if (channel_ && channel_->connected() && downstream_.size() < MAX_DOWNSTREAM_DEPS && !upstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " re-enabling reading from upstream";
    upstream_readable_ = true;
    scoped_refptr<SsConnection> self(this);
    channel_->enable_read([self]() {});
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

void SsConnection::DisableStreamRead() {
  downstream_readable_ = false;
}

void SsConnection::OnDisconnect(asio::error_code ec) {
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
  LOG(INFO) << "Connection (server) " << connection_id()
            << " closed: " << ec << " remaining: " << bytes << " bytes";
  close();
}

void SsConnection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void SsConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    downstream_.push_back(buf);
  }

  if (!downstream_.empty()) {
    WriteStream();
  }
}

void SsConnection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void SsConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    scoped_refptr<SsConnection> self(this);
    channel_->start_write(upstream_.front(), [self](){});
  }
}

void SsConnection::connected() {
  VLOG(2) << "Connection (server) " << connection_id()
          << " remote: established upstream connection with: "
          << remote_domain();
  scoped_refptr<SsConnection> self(this);
  upstream_readable_ = true;
  upstream_writable_ = true;

  channel_->start_read([self](){});
  OnUpstreamWriteFlush();
}

void SsConnection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: received reply: " << buf->length() << " bytes.";

  // queue limit to upstream read
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " disabling reading from upstream";
    upstream_readable_ = false;
    channel_->disable_read();
  }

  if (adapter_) {
    if (padding_support_ && num_padding_send_ < kFirstPaddings) {
      ++num_padding_send_;
      AddPadding(buf);
    }
    data_frame_->AddChunk(buf);
    data_frame_->SetSendCompletionCallback(std::function<void()>());
    adapter()->ResumeStream(stream_id_);
    SendIfNotProcessing();
  } else if (https_fallback_) {
    downstream_.push_back(buf);
  } else {
    downstream_.push_back(EncryptData(buf));
  }
  OnDownstreamWriteFlush();
}

void SsConnection::sent(std::shared_ptr<IOBuf> buf, size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: sent request: " << bytes_transferred << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  upstream_writable_ = true;

  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();

  if (upstream_.size() < MAX_UPSTREAM_DEPS && !downstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " re-enabling reading";
    EnableStreamRead();
  }
}

void SsConnection::disconnected(asio::error_code ec) {
  VLOG(2) << "Connection (server) " << connection_id()
          << " upstream: lost connection with: " << remote_domain()
          << " due to " << ec
          << " and data to write: " << downstream_.size();
  upstream_readable_ = false;
  upstream_writable_ = false;
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  bool nodata = !data_frame_ || !data_frame_->SelectPayloadLength(1).first;
  if (nodata && downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: last data sent: shutting down";
    s_shutdown_(ec);
  }
}

std::shared_ptr<IOBuf> SsConnection::EncryptData(std::shared_ptr<IOBuf> plainbuf) {
  std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(plainbuf->length() + 100);

  encoder_->encrypt(plainbuf.get(), &cipherbuf);
  MSAN_CHECK_MEM_IS_INITIALIZED(cipherbuf->data(), cipherbuf->length());
  return cipherbuf;
}

}  // namespace ss
