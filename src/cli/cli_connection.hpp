// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CLI_CONNECTION
#define H_CLI_CONNECTION

#include "cli/cli_connection_stats.hpp"
#include "core/logging.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/channel.hpp"
#include "net/cipher.hpp"
#include "net/connection.hpp"
#include "net/io_queue.hpp"
#include "net/iobuf.hpp"
#include "net/protocol.hpp"
#include "net/resolver.hpp"
#include "net/socks4.hpp"
#include "net/socks4_request.hpp"
#include "net/socks5.hpp"
#include "net/socks5_request.hpp"
#include "net/ss_request.hpp"
#include "net/ssl_stream.hpp"
#include "net/stream.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/string_view.h>
#include <deque>

#ifdef HAVE_QUICHE
#ifdef HAVE_NGHTTP2
#include <quiche/http2/adapter/nghttp2_adapter.h>
#else
#include <quiche/http2/adapter/oghttp2_adapter.h>
#endif
#endif

namespace net::cli {

#ifdef HAVE_QUICHE
using StreamId = http2::adapter::Http2StreamId;
template <typename T>
using StreamMap = absl::flat_hash_map<StreamId, T>;
#endif  // HAVE_QUICHE

#ifdef HAVE_QUICHE
class CliConnection;
class DataFrameSource : public http2::adapter::DataFrameSource {
 public:
  explicit DataFrameSource(CliConnection* connection) : connection_(connection) {}
  ~DataFrameSource() override = default;
  DataFrameSource(const DataFrameSource&) = delete;
  DataFrameSource& operator=(const DataFrameSource&) = delete;

  void set_stream_id(StreamId stream_id) { stream_id_ = stream_id; }

  std::pair<int64_t, bool> SelectPayloadLength(size_t max_length) override {
    if (chunks_.empty())
      return {kBlocked, last_frame_};

    bool finished = (chunks_.size() <= 1) && (chunks_.front()->length() <= max_length) && last_frame_;

    return {std::min(chunks_.front()->length(), max_length), finished};
  }

  bool Send(absl::string_view frame_header, size_t payload_length) override;

  bool send_fin() const override { return true; }

  void AddChunk(std::shared_ptr<IOBuf> chunk) { chunks_.push_back(std::move(chunk)); }
  void set_last_frame(bool last_frame) { last_frame_ = last_frame; }
  void SetSendCompletionCallback(std::function<void()> callback) { send_completion_callback_ = std::move(callback); }

  size_t empty() const { return chunks_.empty(); }

 private:
  CliConnection* const connection_;
  StreamId stream_id_;
  std::deque<std::shared_ptr<IOBuf>> chunks_;
  bool last_frame_ = false;
  std::function<void()> send_completion_callback_;
};
#endif  // HAVE_QUICHE

/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class CliConnection : public RefCountedThreadSafe<CliConnection>,
#ifdef HAVE_QUICHE
                      public http2::adapter::Http2VisitorInterface,
#endif  // HAVE_QUICHE
                      public Channel,
                      public Connection,
                      public cipher_visitor_interface {
 public:
  static constexpr const ConnectionFactoryType Type = CONNECTION_FACTORY_CLIENT;
  static constexpr const std::string_view Name = "client";

 public:
  /// The state of service
  enum state {
    state_error,
    state_method_select,    /* handshake with socks5 method extension */
    state_socks5_handshake, /* handshake with socks5 destination */
    state_socks4_handshake, /* handshake with socks4/socks4a */
    state_http_handshake,   /* handshake with http */
    state_stream,
  };

  /// Convert the state of service into string
  static const char* state_to_str(enum state state) {
    switch (state) {
      case state_error:
        return "error";
      case state_method_select:
        return "method_select";
      case state_socks5_handshake:
        return "s5handshake";
      case state_socks4_handshake:
        return "s4handshake";
      case state_http_handshake:
        return "hhandshake";
      case state_stream:
        return "stream";
    }
    return "unknown";
  }

  /// Construct the service with io context and socket
  ///
  /// \param io_context the io context associated with the service
  /// \param remote_host_ips the ip addresses used with remote endpoint
  /// \param remote_host_sni the sni name used with remote endpoint
  /// \param remote_port the port used with remote endpoint
  /// \param upstream_https_fallback the data channel (upstream) falls back to https (alpn)
  /// \param https_fallback the data channel falls back to https (alpn)
  /// \param enable_upstream_tls the underlying data channel (upstream) is using tls
  /// \param enable_tls the underlying data channel is using tls
  /// \param upstream_ssl_ctx the ssl context object for tls data transfer (upstream)
  /// \param ssl_ctx the ssl context object for tls data transfer
  CliConnection(asio::io_context& io_context,
                std::string_view remote_host_ips,
                std::string_view remote_host_sni,
                uint16_t remote_port,
                bool upstream_https_fallback,
                bool https_fallback,
                bool enable_upstream_tls,
                bool enable_tls,
                SSL_CTX* upstream_ssl_ctx,
                SSL_CTX* ssl_ctx);

  /// Destruct the service
  ~CliConnection() override;

  CliConnection(const CliConnection&) = delete;
  CliConnection& operator=(const CliConnection&) = delete;

  CliConnection(CliConnection&&) = delete;
  CliConnection& operator=(CliConnection&&) = delete;

  /// Enter the start phase, begin to read requests
  void start();

  /// Close the socket and clean up
  void close();

 private:
  /// flag to mark connection is closed
  bool closed_ = true;

  /// flag to mark connection is shut down
  bool shutdown_ = false;

#ifdef HAVE_QUICHE
 private:
  bool http2_in_recv_callback_ = false;
  void SendIfNotProcessing();
  bool processing_responses_ = false;
  StreamId stream_id_ = 0;
  DataFrameSource* data_frame_ = nullptr;

 public:
  StreamId blocked_stream_ = 0;
#endif  //  HAVE_QUICHE

 public:
  // cipher_visitor_interface
  bool on_received_data(std::shared_ptr<IOBuf> buf) override;
  void on_protocol_error() override;

#ifdef HAVE_QUICHE
 public:
  // http2::adapter::Http2VisitorInterface
  //
  // OnFrameHeader(0, 0, SETTINGS, 0)
  // OnSettingsStart()
  // OnSettingsEnd()
  // // Stream 1
  // OnFrameHeader(1, _, HEADERS, 0x5)
  // OnBeginHeadersForStream(1)
  // OnHeaderForStream(1, _, _) x 4
  // OnEndHeadersForStream(1)
  // OnEndStream(1)
  int64_t OnReadyToSend(absl::string_view serialized) override;
  OnHeaderResult OnHeaderForStream(StreamId stream_id, absl::string_view key, absl::string_view value) override;
  bool OnEndHeadersForStream(StreamId stream_id) override;
  bool OnEndStream(StreamId stream_id) override;
  bool OnCloseStream(StreamId stream_id, http2::adapter::Http2ErrorCode error_code) override;
  // Unused functions
  void OnConnectionError(ConnectionError /*error*/) override;
  bool OnFrameHeader(StreamId /*stream_id*/, size_t /*length*/, uint8_t /*type*/, uint8_t /*flags*/) override;
  void OnSettingsStart() override {}
  void OnSetting(http2::adapter::Http2Setting setting) override {}
  void OnSettingsEnd() override {}
  void OnSettingsAck() override {}
  bool OnBeginHeadersForStream(StreamId stream_id) override;
  bool OnBeginDataForStream(StreamId stream_id, size_t payload_length) override;
  bool OnDataForStream(StreamId stream_id, absl::string_view data) override;
  bool OnDataPaddingLength(StreamId stream_id, size_t padding_length) override;
  void OnRstStream(StreamId stream_id, http2::adapter::Http2ErrorCode error_code) override;
  void OnPriorityForStream(StreamId stream_id, StreamId parent_stream_id, int weight, bool exclusive) override {}
  void OnPing(http2::adapter::Http2PingId ping_id, bool is_ack) override {}
  void OnPushPromiseForStream(StreamId stream_id, StreamId promised_stream_id) override {}
  bool OnGoAway(StreamId last_accepted_stream_id,
                http2::adapter::Http2ErrorCode error_code,
                absl::string_view opaque_data) override;
  void OnWindowUpdate(StreamId stream_id, int window_increment) override {}
  int OnBeforeFrameSent(uint8_t frame_type, StreamId stream_id, size_t length, uint8_t flags) override;
  int OnFrameSent(uint8_t frame_type, StreamId stream_id, size_t length, uint8_t flags, uint32_t error_code) override;
  bool OnInvalidFrame(StreamId stream_id, InvalidFrameError error) override;
  void OnBeginMetadataForStream(StreamId stream_id, size_t payload_length) override {}
  bool OnMetadataForStream(StreamId stream_id, absl::string_view metadata) override;
  bool OnMetadataEndForStream(StreamId stream_id) override;
  void OnErrorDebug(absl::string_view message) override {}
#ifdef HAVE_NGHTTP2
  http2::adapter::NgHttp2Adapter* adapter() { return adapter_.get(); }
#else
  http2::adapter::OgHttp2Adapter* adapter() { return adapter_.get(); }
#endif

#endif

 private:
  /// Get the state machine to the given state
  /// state(Read)            state(Write)
  /// method_select->ReadMethodSelect
  ///                        method_select->WriteMethodSelect
  /// handshake->ReadHandshake
  ///          ->PerformCmdOps
  ///                        handshake->WriteHandShake
  /// stream->ReadStream
  ///                        stream->WriteUpstream
  /// stream->ReadUpstream
  ///                        stream->WriteStream
  ///
  /// Return current state of service
  state CurrentState() const { return state_; }
  /// Set the state machine to the given state
  /// \param nextState the state the service would be set to
  void SetState(state nextState) { state_ = nextState; }

  /// Start to read socks5 method select/socks4 handshake/http handshake request
  void ReadMethodSelect();
  /// Start to read socks5 handshake request
  void ReadSocks5Handshake();

  /// Start to read redir request
  asio::error_code OnReadRedirHandshake(std::shared_ptr<IOBuf> buf);
  /// Start to read socks5 method_select request
  asio::error_code OnReadSocks5MethodSelect(std::shared_ptr<IOBuf> buf);
  /// Start to read socks5 handshake request
  asio::error_code OnReadSocks5Handshake(std::shared_ptr<IOBuf> buf);
  /// Start to read socks4 handshake request
  asio::error_code OnReadSocks4Handshake(std::shared_ptr<IOBuf> buf);
  /// Start to read http handshake request
  asio::error_code OnReadHttpRequest(std::shared_ptr<IOBuf> buf);

  /// Start to read stream
  void ReadStream(bool yield);

  /// write method select response
  void WriteMethodSelect();
  /// write handshake response
  /// \param reply the reply used to write to socket
  void WriteHandshake();
  /// write to stream
  void WriteStream();
  /// write to stream (on writable event)
  void WriteStreamAsync();

  /// Read remaining buffers from upstream
  void ReadUpstream();
  /// Read remaining buffers from upstream (on readable event)
  void ReadUpstreamAsync(bool yield);

  /// Get next remaining buffer to stream
  std::shared_ptr<IOBuf> GetNextDownstreamBuf(asio::error_code& ec, size_t* bytes_transferred);

  /// Write remaining buffers to channel
  void WriteUpstreamInPipe();
  /// Get next remaining buffer to channel
  std::shared_ptr<IOBuf> GetNextUpstreamBuf(asio::error_code& ec, size_t* bytes_transferred, bool* upstream_blocked);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsV5(const socks5::request* request, socks5::reply* reply);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsV4(const socks4::request* request, socks4::reply* reply);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsHttp();

  /// Process the recevied data
  /// \param buf pointer to received buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessReceivedData(std::shared_ptr<IOBuf> buf, asio::error_code error, size_t bytes_transferred);
  /// Process the sent data
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessSentData(asio::error_code error, size_t bytes_transferred);
  /// state machine
  state state_;

 private:
  /// copy of handshake request
  socks5::request s5_request_;

  /// copy of method select response
  socks5::method_select_response method_select_reply_;
  /// copy of handshake response
  socks5::reply s5_reply_;

  /// copy of handshake request
  socks4::request s4_request_;

  /// copy of handshake response
  socks4::reply s4_reply_;

  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of connect method
  bool http_is_connect_ = false;
  /// copy of connect response
  static const std::string_view http_connect_reply_;
  /// copy of keep alive state
  bool http_is_keep_alive_ = false;
  /// copy of remaining bytes in keep alive cycle
  int64_t http_keep_alive_remaining_bytes_ = 0;

  /// copy of upstream request
  std::unique_ptr<ss::request> ss_request_;
  /// copy of padding support
  bool padding_support_ = false;
  int num_padding_send_ = 0;
  int num_padding_recv_ = 0;
  std::shared_ptr<IOBuf> padding_in_middle_buf_;

 private:
  void ReadUpstreamHttpsHandshake(std::shared_ptr<IOBuf> buf, asio::error_code& ec);
  void ReadUpstreamHttpsChunk(std::shared_ptr<IOBuf> buf, asio::error_code& ec);

  void WriteUpstreamMethodSelectRequest();
  void ReadUpstreamMethodSelectResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec);
  void WriteUpstreamAuthRequest();
  void ReadUpstreamAuthResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec);
  void WriteUpstreamSocks4Request();
  void WriteUpstreamSocks4ARequest();
  void WriteUpstreamSocks5Request();
  void ReadUpstreamSocksResponse(std::shared_ptr<IOBuf> buf, asio::error_code& ec);

  /// the state of https fallback handshake (upstream)
  bool upstream_https_handshake_ = true;
  /// the state of https chunked (upstream)
  bool upstream_https_chunked_ = false;
  /// the state of socks5 method select handshake (upstream)
  bool socks5_method_select_handshake_ = false;
  bool socks5_auth_handshake_ = false;
  bool socks_handshake_ = false;

  std::string remote_domain() const {
    std::ostringstream ss;
    if (ss_request_->address_type() == ss::domain) {
      ss << ss_request_->domain_name() << ":" << ss_request_->port();
    } else {
      ss << ss_request_->endpoint();
    }
    return ss.str();
  }

 private:
  /// used to resolve local and remote endpoint
  net::Resolver resolver_;

 private:
  /// perform cmd connect request
  void OnCmdConnect(const asio::ip::tcp::endpoint& endpoint);
  void OnCmdConnect(const std::string& domain_name, uint16_t port);

  /// handle with connnect event (downstream)
  void OnConnect();

  /// handle the read data from stream read event (downstream)
  void OnStreamRead(std::shared_ptr<IOBuf> buf);

  /// handle the written data from stream write event (downstream)
  void OnStreamWrite();

  /// handle with disconnect event (downstream)
  void OnDisconnect(asio::error_code error);

  /// flush downstream and try to write if any in queue
  void OnDownstreamWriteFlush();

  /// write the given data to downstream
  void OnDownstreamWrite(std::shared_ptr<IOBuf> buf);

  /// flush upstream and try to write if any in queue
  void OnUpstreamWriteFlush();

  /// write the given data to upstream
  void OnUpstreamWrite(std::shared_ptr<IOBuf> buf);

  /// the queue to write upstream
  IoQueue upstream_;
  /// the flag to mark current write
  bool upstream_writable_ = false;
  /// the flag to mark current read
  bool upstream_readable_ = false;
  /// the previous read error (upstream)
  asio::error_code pending_upstream_read_error_;

  /// the upstream the service bound with
  scoped_refptr<stream> channel_;

#ifdef HAVE_QUICHE
  /// the http2 upstream adapter
#ifdef HAVE_NGHTTP2
  std::unique_ptr<http2::adapter::NgHttp2Adapter> adapter_;
#else
  std::unique_ptr<http2::adapter::OgHttp2Adapter> adapter_;
#endif
  absl::flat_hash_map<std::string, std::string> request_map_;
#endif

  /// the queue to write downstream
  IoQueue downstream_;
  /// the flag to mark current read
  bool downstream_readable_ = false;
  /// the flag to mark current read in progress
  bool downstream_read_inprogress_ = false;
  /// the previous read error (downstream)
  asio::error_code pending_downstream_read_error_;

 private:
  /// handle with connnect event (upstream)
  void connected();

  /// handle data read event (upstream)
  void received();

  /// handle data write (upstream)
  void sent();

  /// handle with disconnect event (upstream)
  void disconnected(asio::error_code error) override;

 private:
  /// pending data
  IoQueue pending_data_;

  /// encrypt data
  void EncryptData(IoQueue* queue, std::shared_ptr<IOBuf> plaintext);

  /// encode cipher to perform data encoder for upstream
  std::unique_ptr<cipher> encoder_;
  /// decode cipher to perform data decoder from upstream
  std::unique_ptr<cipher> decoder_;

  /// mark of in-progress writing
  bool write_inprogress_ = false;

  friend class DataFrameSource;
};

}  // namespace net::cli

#endif  // H_CLI_CONNECTION
