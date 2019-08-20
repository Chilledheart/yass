//
// socks5_connection.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_SOCKS5_CONNECTION
#define H_SOCKS5_CONNECTION

#include "channel.hpp"
#include "iobuf.hpp"
#include "protocol.hpp"
#include "service.hpp"
#include "socks5.hpp"
#include "socks5_request.hpp"
#include "socks5_request_parser.hpp"
#include "ss_stream.hpp"

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <deque>
#include <glog/logging.h>

class cipher;
namespace socks5 {
/// The ultimate service class to deliever the network traffic to the remote
/// endpoint
class Socks5Connection : public std::enable_shared_from_this<Socks5Connection>,
                         public Channel {
public:
  /// The state of service
  enum state {
    state_error,
    state_method_select, /* handshake with method extension */
    state_handshake,     /* handshake with destination */
    state_stream,
  };

  /// Convert the state of service into string
  static const char *state_to_str(enum state state) {
    switch (state) {
    case state_error:
      return "error";
    case state_method_select:
      return "method_select";
    case state_handshake:
      return "handshake";
    case state_stream:
      return "stream";
    }
    return "unknown";
  }

  /// Return current state of service
  state currentState() const { return state_; }

  /// Construct the service with io context and socket
  ///
  /// \param io_context the io context associated with the service
  /// \param socket the socket bound to the service
  /// \param endpoint the service socket's endpoint
  /// \param peer_endpoint the peer endpoint
  /// \param remote_endpoint the upstream's endpoint
  Socks5Connection(boost::asio::io_context &io_context,
                   boost::asio::ip::tcp::socket &&socket,
                   const boost::asio::ip::tcp::endpoint &endpoint,
                   const boost::asio::ip::tcp::endpoint &peer_endpoint,
                   const boost::asio::ip::tcp::endpoint &remote_endpoint);

  /// Destruct the service
  ~Socks5Connection();

  /// Enter the start phase, begin to read requests
  void start();

  /// Cancel all asynchronous operations on the service
  void cancel() {
    boost::system::error_code ec;
    socket_.cancel(ec);
  }

  /// Close the socket and clean up
  void close() {
    LOG(WARNING) << "disconnected with client at stage: "
                 << Socks5Connection::state_to_str(currentState());
    if (!socket_.is_open()) {
      return;
    }
    boost::system::error_code ec;
    socket_.close(ec);
    auto cb = std::move(disconnect_cb_);
    cb();
    channel_->cancel();
  }

  /// set callback
  void set_disconnect_cb(std::function<void()> cb) { disconnect_cb_ = cb; }

private:
  /// dispatch the command to delegate
  boost::system::error_code
  performCmdOps(const std::shared_ptr<Socks5Connection> &delegate,
                command_type command, reply *reply);

  /// perform cmd connect request
  boost::system::error_code onCmdConnect(ByteRange vaddress, reply *reply);

  /// handle with connnect event (downstream)
  void onConnect();

  /// handle the read data from stream read event (downstream)
  void onStreamRead(std::shared_ptr<IOBuf> buf);

  /// handle the written data from stream write event (downstream)
  void onStreamWrite(std::shared_ptr<IOBuf> buf);

  /// handle with disconnect event (downstream)
  void onDisconnect(boost::system::error_code error);

  /// flush downstream and try to write if any in queue
  void performDownstreamFlush();

  /// write the given data to downstream
  void performDownstreamWrite(std::shared_ptr<IOBuf> buf);

  /// write the given data to upstream
  void performUpstreamWrite(std::shared_ptr<IOBuf> buf);

  /// flush upstream and try to write if any in queue
  void performUpstreamFlush();

private:
  /// Start the read procedure
  void start_read();

  /// Write the given buf with the endpoint
  ///
  /// \param buf the buffer used to write to socket
  void start_write(std::shared_ptr<IOBuf> buf);

  /// set the state machine to the given state
  /// \param nextState the state the service would be set to
  void setState(state nextState) { state_ = nextState; }

  /// read method select request
  void readMethodSelect();
  /// read handshake request
  void readHandshake();
  /// read from stream
  void readStream();

  /// write method select response
  void writeMethodSelect();
  /// write handshake response
  void writeHandshake(reply *reply);
  /// write to stream
  void writeStream(std::shared_ptr<IOBuf> buf);

  /// process the recevied data
  void processReceivedData(const std::shared_ptr<Socks5Connection> &delegate,
                           std::shared_ptr<IOBuf> buf,
                           boost::system::error_code error,
                           size_t bytes_transferred);
  /// process the sent data
  void processSentData(const std::shared_ptr<Socks5Connection> &delegate,
                       std::shared_ptr<IOBuf> buf,
                       boost::system::error_code error,
                       size_t bytes_transferred);

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
  /// the io context associated with
  boost::asio::io_context &io_context_;
  /// the socket the service bound with
  boost::asio::ip::tcp::socket socket_;

  /// the callback invoked when disconnect event happens
  std::function<void()> disconnect_cb_;

  struct {
    state state_;

    method_select_request_parser method_select_request_parser_;
    method_select_request method_select_request_;

    request_parser request_parser_;
    request request_;

    method_select_response method_select_reply_;
    reply reply_;

    std::unique_ptr<ss::request> ss_request_;

    /// Buffer for incoming data.
    std::array<char, 8192> buffer_;
  };

  /// the queue to write upstream
  std::deque<std::shared_ptr<IOBuf>> upstream_;
  /// the flag to mark current write
  bool upstream_writable_;

  /// service's bound endpoint
  const boost::asio::ip::tcp::endpoint endpoint_;
  /// the peer endpoint the connection connects
  const boost::asio::ip::tcp::endpoint peer_endpoint_;
  /// the upstream endpoint to be established with
  const boost::asio::ip::tcp::endpoint remote_endpoint_;

  /// the upstream the service bound with
  std::unique_ptr<ss::stream> channel_;
  /// the encode cipher
  std::unique_ptr<cipher> encoder_;
  /// the decode cipher
  std::unique_ptr<cipher> decoder_;

  /// the queue to write downstream
  std::deque<std::shared_ptr<IOBuf>> downstream_;
  /// the flag to mark current write
  bool downstream_writable_;

  /// read statistics number
  size_t rbytes_transferred_ = 0;
  /// write statistics number
  size_t wbytes_transferred_ = 0;
};

} // namespace socks5

#endif // H_SOCKS5_CONNECTION
