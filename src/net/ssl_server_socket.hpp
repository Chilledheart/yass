// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef H_NET_SSL_SERVER_SOCKET
#define H_NET_SSL_SERVER_SOCKET

#include <absl/functional/any_invocable.h>
#include <openssl/ssl.h>

#include "core/asio.hpp"
#include "core/iobuf.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/openssl_util.hpp"
#include "net/net_errors.hpp"

namespace net {

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
using CompletionOnceCallback = absl::AnyInvocable<void(int)>;
using WaitCallback = absl::AnyInvocable<void(asio::error_code ec)>;

class SSLServerSocket : public RefCountedThreadSafe<SSLServerSocket> {
 public:
  SSLServerSocket(asio::io_context *io_context,
                  asio::ip::tcp::socket* socket,
                  SSL_CTX* ssl_ctx);
  ~SSLServerSocket();

  SSLServerSocket(SSLServerSocket&&) = delete;
  SSLServerSocket& operator=(SSLServerSocket&&) = delete;

  template<typename... Args>
  static scoped_refptr<SSLServerSocket> Create(Args&&... args) {
    return MakeRefCounted<SSLServerSocket>(std::forward<Args>(args)...);
  }

  int Handshake(CompletionOnceCallback callback);
  int Shutdown(WaitCallback &&callback, bool force = false);

  // StreamSocket implementation
  void Disconnect();

  SSL* native_handle() { return ssl_.get(); }

  // Socket implementation.
  size_t Read(std::shared_ptr<IOBuf> buf, asio::error_code &ec);
  size_t Write(std::shared_ptr<IOBuf> buf, asio::error_code &ec);
  void WaitRead(WaitCallback &&cb);
  void WaitWrite(WaitCallback &&cb);

 protected:
  void OnWaitRead(asio::error_code ec);
  void OnWaitWrite(asio::error_code ec);
  void OnReadReady();
  void OnWriteReady();
  void OnDoWaitShutdown(asio::error_code ec);

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
  WaitCallback wait_read_callback_;
  WaitCallback wait_write_callback_;
  WaitCallback wait_shutdown_callback_;
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

  // True if the socket has been disconnected.
  bool disconnected_ = false;
};

} // namespace net

#endif // H_NET_SSL_SERVER_SOCKET

