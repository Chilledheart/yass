// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_SOCKS5_CONNECTION
#define H_SOCKS5_CONNECTION

#include "channel.hpp"
#include "cli/socks5_connection_stats.hpp"
#include "connection.hpp"
#include "core/http_parser.h"
#include "core/iobuf.hpp"
#include "core/logging.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "core/socks4.hpp"
#include "core/socks4_request.hpp"
#include "core/socks4_request_parser.hpp"
#include "core/socks5.hpp"
#include "core/socks5_request.hpp"
#include "core/socks5_request_parser.hpp"
#include "core/ss_request.hpp"
#include "protocol.hpp"
#include "stream.hpp"

#include <absl/container/flat_hash_map.h>
#include <deque>

class cipher;
/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class Socks5Connection : public RefCountedThreadSafe<Socks5Connection>,
                         public Channel,
                         public Connection {
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
  /// \param remote_endpoint the upstream's endpoint
  Socks5Connection(asio::io_context& io_context,
                   const asio::ip::tcp::endpoint& remote_endpoint);

  /// Destruct the service
  ~Socks5Connection() override;

  Socks5Connection(const Socks5Connection&) = delete;
  Socks5Connection& operator=(const Socks5Connection&) = delete;

  Socks5Connection(Socks5Connection&&) = delete;
  Socks5Connection& operator=(Socks5Connection&&) = delete;

  /// Enter the start phase, begin to read requests
  void start() override;

  /// Close the socket and clean up
  void close() override;

 private:
  /// flag to mark connection is closed
  bool closed_ = true;

 private:
  /// Get the state machine to the given state
  /// state(Read)            state(Write)
  /// method_select->ReadMethodSelect
  ///                        method_select->WriteMethodSelect
  /// handshake->ReadHandshake
  ///          ->PerformCmdOps
  ///                        handshake->WriteHandShake
  /// stream->ReadStream
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

  /// Callback to read http handshake request's URL field
  static int OnReadHttpRequestURL(http_parser* p, const char* buf, size_t len);
  /// Callback to read http handshake request's URL field
  static int OnReadHttpRequestHeaderField(http_parser* parser,
                                          const char* buf,
                                          size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeaderValue(http_parser* parser,
                                          const char* buf,
                                          size_t len);
  /// Callback to read http handshake request's headers done
  static int OnReadHttpRequestHeadersDone(http_parser* parser);

  /// Start to read stream
  void ReadStream();

  /// write method select response
  void WriteMethodSelect();
  /// write handshake response
  /// \param reply the reply used to write to socket
  void WriteHandshake();
  /// write to stream
  /// \param buf the buffer used to write to socket
  void WriteStream();

  /// Write remaining buffers to stream
  void WriteStreamInPipe();
  /// Get next remaining buffer to stream
  std::shared_ptr<IOBuf> GetNextDownstreamBuf(asio::error_code &ec);

  /// Write remaining buffers to channel
  void WriteUpstreamInPipe();
  /// Get next remaining buffer to channel
  std::shared_ptr<IOBuf> GetNextUpstreamBuf(asio::error_code &ec);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsV5(const socks5::request* request,
                                   socks5::reply* reply);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsV4(const socks4::request* request,
                                   socks4::reply* reply);

  /// dispatch the command to delegate
  /// \param command command type
  /// \param reply reply to given command type
  asio::error_code PerformCmdOpsHttp();

  /// Process the recevied data
  /// \param buf pointer to received buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessReceivedData(std::shared_ptr<IOBuf> buf,
                           asio::error_code error,
                           size_t bytes_transferred);
  /// Process the sent data
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  void ProcessSentData(asio::error_code error,
                       size_t bytes_transferred);
  /// state machine
  state state_;

  /// parser of method select request
  socks5::method_select_request_parser method_select_request_parser_;
  /// copy of method select request
  socks5::method_select_request method_select_request_;

  /// parser of handshake request
  socks5::request_parser request_parser_;
  /// copy of handshake request
  socks5::request s5_request_;

  /// copy of method select response
  socks5::method_select_response method_select_reply_;
  /// copy of handshake response
  socks5::reply s5_reply_;

  /// parser of handshake request
  socks4::request_parser s4_request_parser_;
  /// copy of handshake request
  socks4::request s4_request_;

  /// copy of handshake response
  socks4::reply s4_reply_;

  /// copy of url;
  std::string http_url_;
  /// copy of parsed connect host or host field
  std::string http_host_;
  /// copy of parsed connect host or host field
  uint16_t http_port_ = 0U;
  /// copy of parsed header field
  std::string http_field_;
  /// copy of parsed header value
  std::string http_value_;
  /// copy of parsed headers
  absl::flat_hash_map<std::string, std::string> http_headers_;
  /// copy of connect method
  bool http_is_connect_ = false;
  /// copy of connect response
  static const char http_connect_reply_[];

  /// copy of upstream request
  std::unique_ptr<ss::request> ss_request_;

  std::string remote_domain() const {
    std::stringstream ss;
    if (ss_request_->address_type() == ss::domain) {
      ss << ss_request_->domain_name() << ":" << ss_request_->port();
    } else {
      ss << ss_request_->endpoint();
    }
    return ss.str();
  }

 private:
  /// perform cmd connect request
  void OnCmdConnect(std::shared_ptr<IOBuf> buf);

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

  /// the queue to write upstream
  std::deque<std::shared_ptr<IOBuf>> upstream_;
  /// the flag to mark current write
  bool upstream_writable_ = false;
  /// the flag to mark current read
  bool upstream_readable_ = false;

  /// the upstream the service bound with
  std::unique_ptr<stream> channel_;

  /// the queue to write downstream
  std::deque<std::shared_ptr<IOBuf>> downstream_;
  /// the flag to mark current read
  bool downstream_readable_ = false;
  /// the flag to mark current read in progress
  bool downstream_read_inprogress_ = false;

 private:
  /// handle with connnect event (upstream)
  void connected() override;

  /// handle read data for data read event (upstream)
  void received(std::shared_ptr<IOBuf> buf) override;

  /// handle written data for data sent event (upstream)
  void sent(std::shared_ptr<IOBuf> buf, size_t bytes_transferred) override;

  /// handle with disconnect event (upstream)
  void disconnected(asio::error_code error) override;

 private:
  /// decrypt data
  std::shared_ptr<IOBuf> DecryptData(std::shared_ptr<IOBuf> buf);
  /// encrypt data
  std::shared_ptr<IOBuf> EncryptData(std::shared_ptr<IOBuf> buf);

  /// encode cipher to perform data encoder for upstream
  std::unique_ptr<cipher> encoder_;
  /// decode cipher to perform data decoder from upstream
  std::unique_ptr<cipher> decoder_;

  /// statistics of read bytes (non-encoded)
  size_t rbytes_transferred_ = 0;
  /// statistics of written bytes (non-encoded)
  size_t wbytes_transferred_ = 0;
};

class Socks5ConnectionFactory : public ConnectionFactory {
 public:
   using ConnectionType = Socks5Connection;
   scoped_refptr<ConnectionType> Create(asio::io_context& io_context,
                                        const asio::ip::tcp::endpoint& remote_endpoint) {
     return MakeRefCounted<ConnectionType>(io_context, remote_endpoint);
   }
   const char* Name() override { return "client"; };
   const char* ShortName() override { return "client"; };
};

#endif  // H_SOCKS5_CONNECTION
