//
// ss_connection.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SS_CONNECTION
#define H_SS_CONNECTION

#include "channel.hpp"
#include "connection.hpp"
#include "iobuf.hpp"
#include "protocol.hpp"
#include "ss.hpp"
#include "ss_request.hpp"
#include "ss_request_parser.hpp"
#include "ss_stream.hpp"

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <deque>
#include <glog/logging.h>

class cipher;
namespace ss {
/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class SsConnection : public std::enable_shared_from_this<SsConnection>,
                     public Channel,
                     public Connection {
public:
  /// The state of service
  enum state {
    state_error,
    state_handshake, /* handshake with destination */
    state_stream,
  };

  /// Convert the state of service into string
  static const char *state_to_str(enum state state) {
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
  SsConnection(boost::asio::io_context &io_context,
               const boost::asio::ip::tcp::endpoint &remote_endpoint);

  /// Destruct the service
  ~SsConnection();

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
  /// Start to read stream
  void ReadStream();

  /// write to stream
  /// \param buf the buffer used to write to socket
  void WriteStream(std::shared_ptr<IOBuf> buf);

  /// Process the recevied data
  /// \param self pointer to self
  /// \param buf pointer to received buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  static void ProcessReceivedData(std::shared_ptr<SsConnection> self,
                                  std::shared_ptr<IOBuf> buf,
                                  boost::system::error_code error,
                                  size_t bytes_transferred);
  /// Process the sent data
  /// \param self pointer to self
  /// \param buf pointer to sent buffer
  /// \param error the error state
  /// \param bytes_transferred transferred bytes
  static void ProcessSentData(std::shared_ptr<SsConnection> self,
                              std::shared_ptr<IOBuf> buf,
                              boost::system::error_code error,
                              size_t bytes_transferred);
  /// state machine
  state state_;

  /// parser of handshake request
  request_parser request_parser_;
  /// copy of handshake request
  request request_;

private:
  /// handle with connnect event (downstream)
  void OnConnect();

  /// handle the read data from stream read event (downstream)
  void OnStreamRead(std::shared_ptr<IOBuf> buf);

  /// handle the written data from stream write event (downstream)
  void OnStreamWrite(std::shared_ptr<IOBuf> buf);

  /// handle with disconnect event (downstream)
  void OnDisconnect(boost::system::error_code error);

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

  /// the upstream the service bound with
  std::unique_ptr<ss::stream> channel_;

  /// the queue to write downstream
  std::deque<std::shared_ptr<IOBuf>> downstream_;
  /// the flag to mark current write
  bool downstream_writable_ = false;

private:
  /// handle with connnect event (upstream)
  void connected() override;

  /// handle read data for data read event (upstream)
  void received(std::shared_ptr<IOBuf> buf) override;

  /// handle written data for data sent event (upstream)
  void sent(std::shared_ptr<IOBuf> buf) override;

  /// handle with disconnect event (upstream)
  void disconnected(boost::system::error_code error) override;

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
  /// statistics of write bytes (non-encoded)
  size_t wbytes_transferred_ = 0;
};

} // namespace ss

#endif // H_SS_CONNECTION
