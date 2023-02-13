// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_NET_SSL_SERVER_SOCKET
#define H_NET_SSL_SERVER_SOCKET

#include "net/socket_bio_adapter.hpp"
#include <openssl/ssl.h>

namespace net {

class SSLServerSocket : public SocketBIOAdapter::Delegate {
 public:
  SSLServerSocket(asio::io_context *io_context,
                  asio::ip::tcp::socket* socket,
                  SSL_CTX* ssl_ctx);
  ~SSLServerSocket();

  SSLServerSocket(SSLServerSocket&&) = default;
  SSLServerSocket& operator=(SSLServerSocket&&) = default;

  int Handshake(CompletionOnceCallback callback);
  int Shutdown();

  // StreamSocket implementation
  void Disconnect();

  SSL* native_handle() { return ssl_.get(); }

  // Socket implementation.
  size_t Read(std::shared_ptr<IOBuf> buf, asio::error_code &ec);
  size_t Write(std::shared_ptr<IOBuf> buf, asio::error_code &ec);

 protected:
  void OnReadReady();
  void OnWriteReady();

 private:
  int DoHandshake();
  void DoHandshakeCallback(int result);

  void OnVerifyComplete(int result);
  void OnHandshakeIOComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len);
  int DoPayloadWrite(std::shared_ptr<IOBuf> buf, int buf_len);
  int MapLastOpenSSLError(int ssl_error);

 private:
  asio::io_context* io_context_;
  asio::ip::tcp::socket* stream_socket_;

  CompletionOnceCallback user_handshake_callback_;
  bool completed_handshake_ = false;
  bool completed_connect_ = false;

  // SSLPrivateKey signature.
  int signature_result_;
  std::vector<uint8_t> signature_;

  // OpenSSL stuff
  bssl::UniquePtr<SSL> ssl_;

  // Whether we received any data in early data.
  bool early_data_received_ = false;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
  };
  State next_handshake_state_ = STATE_NONE;

  std::unique_ptr<SocketBIOAdapter> transport_adapter_;
};

} // namespace net

#endif // H_NET_SSL_SERVER_SOCKET

