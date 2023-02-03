// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef H_SS_CONNECTION
#define H_SS_CONNECTION

#include "channel.hpp"
#include "connection.hpp"
#include "core/cipher.hpp"
#include "core/c-ares.hpp"
#include "core/iobuf.hpp"
#include "core/logging.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "core/ss.hpp"
#include "core/ss_request.hpp"
#include "core/ss_request_parser.hpp"
#include "protocol.hpp"
#include "stream.hpp"
#include "quiche/http2/adapter/oghttp2_adapter.h"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/string_view.h>
#include <absl/strings/str_cat.h>
#include <deque>

class cipher;
namespace ss {
using StreamId = http2::adapter::Http2StreamId;
template <typename T>
using StreamMap = absl::flat_hash_map<StreamId, T>;

class SsConnection;

class DataFrameSource
    : public http2::adapter::DataFrameSource {
 public:
  explicit DataFrameSource(SsConnection* connection,
                           const StreamId& stream_id)
      : connection_(connection), stream_id_(stream_id) {}
  ~DataFrameSource() override = default;
  DataFrameSource(const DataFrameSource&) = delete;
  DataFrameSource& operator=(const DataFrameSource&) = delete;

  std::pair<int64_t, bool> SelectPayloadLength(size_t max_length) override {
    if (chunks_.empty())
      return {kBlocked, last_frame_};

    bool finished = (chunks_.size() <= 1) &&
                    (chunks_.front()->length() <= max_length) && last_frame_;

    return {std::min(chunks_.front()->length(), max_length), finished};
  }

  bool Send(absl::string_view frame_header, size_t payload_length) override;

  bool send_fin() const override { return true; }

  void AddChunk(std::shared_ptr<IOBuf> chunk) { chunks_.push_back(std::move(chunk)); }
  void set_last_frame(bool last_frame) { last_frame_ = last_frame; }
  void SetSendCompletionCallback(std::function<void()> callback) {
    send_completion_callback_ = std::move(callback);
  }

 private:
  SsConnection* const connection_;
  const StreamId stream_id_;
  std::deque<std::shared_ptr<IOBuf>> chunks_;
  bool last_frame_ = false;
  std::function<void()> send_completion_callback_;
};

/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class SsConnection : public RefCountedThreadSafe<SsConnection>,
                     public Channel,
                     public Connection,
                     public cipher_visitor_interface,
                     public http2::adapter::Http2VisitorInterface {
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
  /// \param remote_endpoint the upstream's endpoint
  /// \param remote_host_name the sni name used with remote endpoint
  /// \param upstream_https_fallback the data channel (upstream) falls back to https (alpn)
  /// \param https_fallback the data channel falls back to https (alpn)
  /// \param enable_upstream_tls the underlying data channel (upstream) is using tls
  /// \param enable_tls the underlying data channel is using tls
  /// \param upstream_ssl_ctx the ssl context object for tls data transfer (upstream)
  /// \param ssl_ctx the ssl context object for tls data transfer
  SsConnection(asio::io_context& io_context,
               const asio::ip::tcp::endpoint& remote_endpoint,
               const std::string& remote_host_name,
               bool upstream_https_fallback,
               bool https_fallback,
               bool enable_upstream_tls,
               bool enable_tls,
               asio::ssl::context *upstream_ssl_ctx,
               asio::ssl::context *ssl_ctx);

  /// Destruct the service
  ~SsConnection() override;

  SsConnection(const SsConnection&) = delete;
  SsConnection& operator=(const SsConnection&) = delete;

  SsConnection(SsConnection&&) = delete;
  SsConnection& operator=(SsConnection&&) = delete;

  /// Enter the start phase, begin to read requests
  void start() override;

  /// Close the socket and clean up
  void close() override;

 private:
  /// Enter the start phase
  void Start();

  /// flag to mark connection is closed
  bool closed_ = true;

 private:
  void SendIfNotProcessing();
  bool processing_responses_ = false;
  StreamId stream_id_ = 0;
  DataFrameSource* data_frame_ = nullptr;

 public:
  StreamId blocked_stream_ = 0;

 public:
  // cipher_visitor_interface
  bool on_received_data(std::shared_ptr<IOBuf> buf) override;
  void on_protocol_error() override;

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
  OnHeaderResult OnHeaderForStream(StreamId stream_id,
                                   absl::string_view key,
                                   absl::string_view value) override;
  bool OnEndHeadersForStream(StreamId stream_id) override;
  bool OnEndStream(StreamId stream_id) override;
  bool OnCloseStream(StreamId stream_id,
                     http2::adapter::Http2ErrorCode error_code) override;
  // Unused functions
  void OnConnectionError(ConnectionError /*error*/) override;
  bool OnFrameHeader(StreamId /*stream_id*/,
                     size_t /*length*/,
                     uint8_t /*type*/,
                     uint8_t /*flags*/) override;
  void OnSettingsStart() override {}
  void OnSetting(http2::adapter::Http2Setting setting) override {}
  void OnSettingsEnd() override {}
  void OnSettingsAck() override {}
  bool OnBeginHeadersForStream(StreamId stream_id) override;
  bool OnBeginDataForStream(StreamId stream_id, size_t payload_length) override;
  bool OnDataForStream(StreamId stream_id, absl::string_view data) override;
  bool OnDataPaddingLength(StreamId stream_id, size_t padding_length) override;
  void OnRstStream(StreamId stream_id,
                   http2::adapter::Http2ErrorCode error_code) override {}
  void OnPriorityForStream(StreamId stream_id,
                           StreamId parent_stream_id,
                           int weight,
                           bool exclusive) override {}
  void OnPing(http2::adapter::Http2PingId ping_id, bool is_ack) override {}
  void OnPushPromiseForStream(StreamId stream_id,
                              StreamId promised_stream_id) override {}
  bool OnGoAway(StreamId last_accepted_stream_id,
                http2::adapter::Http2ErrorCode error_code,
                absl::string_view opaque_data) override;
  void OnWindowUpdate(StreamId stream_id, int window_increment) override {}
  int OnBeforeFrameSent(uint8_t frame_type,
                        StreamId stream_id,
                        size_t length,
                        uint8_t flags) override;
  int OnFrameSent(uint8_t frame_type,
                  StreamId stream_id,
                  size_t length,
                  uint8_t flags,
                  uint32_t error_code) override;
  bool OnInvalidFrame(StreamId stream_id, InvalidFrameError error) override;
  void OnBeginMetadataForStream(StreamId stream_id,
                                size_t payload_length) override {}
  bool OnMetadataForStream(StreamId stream_id,
                           absl::string_view metadata) override;
  bool OnMetadataEndForStream(StreamId stream_id) override;
  void OnErrorDebug(absl::string_view message) override {}

  http2::adapter::OgHttp2Adapter* adapter() { return adapter_.get(); }

 private:
  /// Get the state machine to the given state
  /// state(Read)            state(Write)
  /// handshake->ReadHandshake
  ///          ->ResolveDns
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
  /// Start to resolve DNS domain name
  /// \param buf the buffer after domain name
  void ResolveDns(std::shared_ptr<IOBuf> buf);

  /// Start to read stream
  void ReadStream();
  /// Write remaining buffers to stream
  void WriteStream();
  /// Write remaining buffers to stream
  void WriteStreamInPipe();
  /// Get next remaining buffer to stream
  std::shared_ptr<IOBuf> GetNextDownstreamBuf(asio::error_code &ec);

  /// Write remaining buffers to channel
  void WriteUpstreamInPipe();
  /// Get next remaining buffer to channel
  std::shared_ptr<IOBuf> GetNextUpstreamBuf(asio::error_code &ec);

  /// Process the recevied data
  /// \param buf pointer to received buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessReceivedData(std::shared_ptr<IOBuf> buf,
                           asio::error_code ec,
                           size_t bytes_transferred);
  /// Process the sent data
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessSentData(asio::error_code ec,
                       size_t bytes_transferred);
  /// state machine
  state state_;

  /// parser of handshake request
  request_parser request_parser_;
  /// copy of handshake request
  request request_;

  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of connect method
  bool http_is_connect_ = false;
  /// copy of connect response
  static const char http_connect_reply_[];
  /// copy of padding support
  bool padding_support_ = false;
  int num_padding_send_ = 0;
  int num_padding_recv_ = 0;
  std::shared_ptr<IOBuf> padding_in_middle_buf_;

  /// DNS resolver
  scoped_refptr<CAresResolver> resolver_;

  std::string remote_domain() const {
    std::stringstream ss;
    if (request_.address_type() == domain) {
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

  /// enable stream read
  void EnableStreamRead();

  /// disable stream read
  void DisableStreamRead();

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
  std::deque<std::shared_ptr<IOBuf>> upstream_;
  /// the flag to mark current write
  bool upstream_writable_ = false;
  /// the flag to mark current read
  bool upstream_readable_ = false;

  /// the upstream the service bound with
  std::unique_ptr<stream> channel_;

  /// the http2 upstream adapter
  std::unique_ptr<http2::adapter::OgHttp2Adapter> adapter_;
  absl::flat_hash_map<std::string, std::string> request_map_;

  /// the queue to write downstream
  std::deque<std::shared_ptr<IOBuf>> downstream_;
  /// the flag to mark current read
  bool downstream_readable_ = false;
  /// the flag to mark current read in progress
  bool downstream_read_inprogress_ = false;

 private:
  /// handle with connect event (upstream)
  void connected() override;

  /// handle read data for data read event (upstream)
  void received(std::shared_ptr<IOBuf> buf) override;

  /// handle written data for data sent event (upstream)
  void sent(std::shared_ptr<IOBuf> buf, size_t bytes_transferred) override;

  /// handle with disconnect event (upstream)
  void disconnected(asio::error_code error) override;

 private:
  /// encrypt data
  std::shared_ptr<IOBuf> EncryptData(std::shared_ptr<IOBuf> buf);

  /// encode cipher to perform data encoder for upstream
  std::unique_ptr<cipher> encoder_;
  /// decode cipher to perform data decoder from upstream
  std::unique_ptr<cipher> decoder_;

  /// mark of in-progress writing
  bool write_inprogress_ = false;

  /// statistics of read bytes (non-encoded)
  size_t rbytes_transferred_ = 0;
  /// statistics of write bytes (non-encoded)
  size_t wbytes_transferred_ = 0;
};

class SsConnectionFactory : public ConnectionFactory {
 public:
   using ConnectionType = SsConnection;
   scoped_refptr<ConnectionType> Create(asio::io_context& io_context,
                                        const asio::ip::tcp::endpoint& remote_endpoint,
                                        const std::string& remote_host_name,
                                        bool upstream_https_fallback,
                                        bool https_fallback,
                                        bool enable_upstream_tls,
                                        bool enable_tls,
                                        asio::ssl::context *upstream_ssl_ctx,
                                        asio::ssl::context *ssl_ctx) {
     return MakeRefCounted<ConnectionType>(io_context, remote_endpoint, remote_host_name,
                                           upstream_https_fallback, https_fallback,
                                           enable_upstream_tls, enable_tls,
                                           upstream_ssl_ctx, ssl_ctx);
   }
   const char* Name() override { return "server"; };
   const char* ShortName() override { return "server"; };
};

}  // namespace ss

#endif  // H_SS_CONNECTION
