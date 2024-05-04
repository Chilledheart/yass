// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_SS_CONNECTION
#define H_SS_CONNECTION

#include "core/logging.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/channel.hpp"
#include "net/cipher.hpp"
#include "net/connection.hpp"
#include "net/io_queue.hpp"
#include "net/iobuf.hpp"
#include "net/protocol.hpp"
#include "net/ss.hpp"
#include "net/ss_request.hpp"
#include "net/ss_request_parser.hpp"
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

class cipher;
namespace net::server {

#ifdef HAVE_QUICHE
using StreamId = http2::adapter::Http2StreamId;
template <typename T>
using StreamMap = absl::flat_hash_map<StreamId, T>;
#endif

class ServerConnection;

#ifdef HAVE_QUICHE
class DataFrameSource : public http2::adapter::DataFrameSource {
 public:
  explicit DataFrameSource(ServerConnection* connection, const StreamId& stream_id)
      : connection_(connection), stream_id_(stream_id) {}
  ~DataFrameSource() override = default;
  DataFrameSource(const DataFrameSource&) = delete;
  DataFrameSource& operator=(const DataFrameSource&) = delete;

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

 private:
  ServerConnection* const connection_;
  const StreamId stream_id_;
  std::deque<std::shared_ptr<IOBuf>> chunks_;
  bool last_frame_ = false;
  std::function<void()> send_completion_callback_;
};
#endif

/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class ServerConnection : public RefCountedThreadSafe<ServerConnection>,
#ifdef HAVE_QUICHE
                         public http2::adapter::Http2VisitorInterface,
#endif
                         public Channel,
                         public Connection,
                         public cipher_visitor_interface {
 public:
  static constexpr const ConnectionFactoryType Type = CONNECTION_FACTORY_SERVER;
  static constexpr const std::string_view Name = "server";

 public:
  /// The state of service
  enum state {
    state_error,
    state_handshake, /* handshake with destination */
    state_stream,
  };

  /// Convert the state of service into string
  static const char* state_to_str(enum state state) {
    switch (state) {
      case state_error:
        return "error";
      case state_handshake:
        return "handshake";
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
  ServerConnection(asio::io_context& io_context,
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
  ~ServerConnection() override;

  ServerConnection(const ServerConnection&) = delete;
  ServerConnection& operator=(const ServerConnection&) = delete;

  ServerConnection(ServerConnection&&) = delete;
  ServerConnection& operator=(ServerConnection&&) = delete;

  /// Enter the start phase, begin to read requests
  void start();

  /// Close the socket and clean up
  void close();

 private:
  /// Enter the start phase
  void Start();

  /// flag to mark connection is shutdown
  bool shutdown_ = false;

  /// flag to mark connection is closing
  bool closing_ = true;

  /// flag to mark connection is closed
  bool closed_ = true;

#ifdef HAVE_QUICHE
 private:
  void SendIfNotProcessing();
  bool processing_responses_ = false;
  StreamId stream_id_ = 0;
  DataFrameSource* data_frame_ = nullptr;

 public:
  StreamId blocked_stream_ = 0;
#endif

 public:
  // cipher_visitor_interface
  bool on_received_data(std::shared_ptr<IOBuf> buf) override;
  void on_protocol_error() override;

#ifdef HAVE_QUICHE
 public:
  // http2::adapter::Http2VisitorInterface
  // OnBeforeFrameSent(SETTINGS, 0, _, 0x0)
  // OnFrameSent(SETTINGS, 0, _, 0x0, 0)
  // // SETTINGS ack
  // OnBeforeFrameSent(SETTINGS, 0, 0, 0x1)
  // OnFrameSent(SETTINGS, 0, 0, 0x1, 0)
  // // Stream 1, with doomed DATA
  // OnBeforeFrameSent(HEADERS, 1, _, 0x4)
  // OnFrameSent(HEADERS, 1, _, 0x4, 0)
  // OnFrameSent(DATA, 1, _, 0x0, 0)
  // OnConnectionError(ConnectionError::kSendError)
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
  /// handshake->ReadHandshake
  /// stream->ReadStream
  ///                        stream->WriteStream
  ///
  /// Return current state of service
  state CurrentState() const { return state_; }
  /// Set the state machine to the given state
  /// \param nextState the state the service would be set to
  void SetState(state nextState) { state_ = nextState; }

  /// Start to read handshake request
  void ReadHandshake();
  /// Start to read handshake request (via https fallback)
  void ReadHandshakeViaHttps();
  void OnReadHandshakeViaHttps();

  /// Start to read stream
  void ReadStream(bool yield);
  /// Write remaining buffers to stream
  void WriteStream();
  /// Write remaining buffers to stream
  void WriteStreamInPipe();
  /// Get next remaining buffer to stream
  std::shared_ptr<IOBuf> GetNextDownstreamBuf(asio::error_code& ec, size_t* bytes_transferred);

  /// Write remaining buffers to channel
  void WriteUpstreamInPipe();
  /// Get next remaining buffer to channel
  std::shared_ptr<IOBuf> GetNextUpstreamBuf(asio::error_code& ec, size_t* bytes_transferred);

  /// Process the recevied data
  /// \param buf pointer to received buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessReceivedData(std::shared_ptr<IOBuf> buf, asio::error_code ec, size_t bytes_transferred);
  /// Process the sent data
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessSentData(asio::error_code ec, size_t bytes_transferred);
  /// state machine
  state state_;

  /// parser of handshake request
  ss::request_parser request_parser_;
  /// copy of handshake request
  ss::request request_;

  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of connect method
  bool http_is_connect_ = false;
  /// copy of connect response
  static const std::string_view http_connect_reply_;
  /// copy of padding support
  bool padding_support_ = false;
  int num_padding_send_ = 0;
  int num_padding_recv_ = 0;
  std::shared_ptr<IOBuf> padding_in_middle_buf_;

  std::string remote_domain() const {
    std::ostringstream ss;
    if (request_.address_type() == ss::domain) {
      ss << request_.domain_name() << ":" << request_.port();
    } else {
      ss << request_.endpoint();
    }
    return ss.str();
  }

 private:
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

  /// buffer of handshake header
  std::shared_ptr<IOBuf> handshake_;
  /// the queue to write upstream
  IoQueue upstream_;
  /// the flag to mark current write
  bool upstream_writable_ = false;
  /// the flag to mark current read
  bool upstream_readable_ = false;
  /// the previous read error (upstream)
  asio::error_code pending_upstream_read_error_;
  /// the previous written bytes
  size_t bytes_upstream_passed_without_yield_ = 0U;
  /// the time to yield after previous write
  uint64_t yield_upstream_after_time_ = 0U;

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
  /// the previous written bytes
  size_t bytes_downstream_passed_without_yield_ = 0U;
  /// the time to yield after previous write
  uint64_t yield_downstream_after_time_ = 0U;

 private:
  /// handle with connect event (upstream)
  void connected();

  /// handle data read event (upstream)
  void received();

  /// handle data write (upstream)
  void sent();

  /// handle with disconnect event (upstream)
  void disconnected(asio::error_code error) override;

 private:
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

}  // namespace net::server

#endif  // H_SS_CONNECTION
