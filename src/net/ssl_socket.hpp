// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_NET_SSL_SOCKET
#define H_NET_SSL_SOCKET

#include "net/socket_bio_adapter.hpp"
#include <openssl/ssl.h>

namespace net {

class SSLSocket : public SocketBIOAdapter::Delegate {
 public:
  SSLSocket(asio::io_context *io_context,
            asio::ip::tcp::socket* socket,
            SSL_CTX* ssl_ctx,
            bool https_fallback,
            const std::string& host_name);

  // StreamSocket implementation
  int Connect(CompletionOnceCallback callback);
  void Disconnect();
  int ConfirmHandshake(CompletionOnceCallback callback);

  SSL* native_handle() { return ssl_.get(); }

  // Socket implementation.
  size_t Read(std::shared_ptr<IOBuf> buf, asio::error_code &ec);
  size_t Write(std::shared_ptr<IOBuf> buf, asio::error_code &ec);

 protected:
  void OnReadReady();
  void OnWriteReady();

 private:
  int DoHandshake();
  int DoHandshakeComplete(int result);
  void DoConnectCallback(int result);

  void OnVerifyComplete(int result);
  void OnHandshakeIOComplete(int result);
  void RetryAllOperations();

  int DoHandshakeLoop(int last_io_result);
  int DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len);
  int DoPayloadWrite(std::shared_ptr<IOBuf> buf, int buf_len);
  void DoPeek();
  int MapLastOpenSSLError(int ssl_error);

 private:
  asio::io_context* io_context_;
  asio::ip::tcp::socket* stream_socket_;

  CompletionOnceCallback user_connect_callback_;

  bool first_post_handshake_write_ = true;

  // True if early data enabled
  bool early_data_enabled_ = false;
  // True if we've already handled the result of our attempt to use early data.
  bool handled_early_data_result_ = false;

  // Used by DoPayloadRead() when attempting to fill the caller's buffer with
  // as much data as possible without blocking.
  // If DoPayloadRead() encounters an error after having read some data, stores
  // the result to return on the *next* call to DoPayloadRead().  A value > 0
  // indicates there is no pending result, otherwise 0 indicates EOF and < 0
  // indicates an error.
  int pending_read_error_;

  // If there is a pending read result, the OpenSSL result code (output of
  // SSL_get_error) associated with it.
  int pending_read_ssl_error_ = SSL_ERROR_NONE;

  // Set when Connect finishes.
  bool completed_connect_ = false;

  // Set when Read() or Write() successfully reads or writes data to or from the
  // network.
  bool was_ever_used_ = false;

  // OpenSSL stuff
  bssl::UniquePtr<SSL> ssl_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_HANDSHAKE_COMPLETE,
  };
  State next_handshake_state_ = STATE_NONE;

  // True if we are currently confirming the handshake.
  bool in_confirm_handshake_ = false;

  // True if the post-handshake SSL_peek has completed.
  bool peek_complete_ = false;

  // True if the socket has been disconnected.
  bool disconnected_ = false;

  // True if there was a certificate error which should be treated as fatal,
  // and false otherwise.
  bool is_fatal_cert_error_ = false;

  // True if the socket should respond to client certificate requests with
  // |client_cert_| and |client_private_key_|, which may be null to continue
  // with no certificate. If false, client certificate requests will result in
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  bool send_client_cert_;

  std::unique_ptr<SocketBIOAdapter> transport_adapter_;
};

} // namespace net

#endif // H_NET_SSL_SOCKET
