// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#ifndef H_NET_SSL_SOCKET
#define H_NET_SSL_SOCKET

#include <absl/functional/any_invocable.h>
#include <string_view>
#include "third_party/boringssl/src/include/openssl/ssl.h"

#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "net/asio.hpp"
#include "net/iobuf.hpp"
#include "net/net_errors.hpp"
#include "net/openssl_util.hpp"
#include "net/protocol.hpp"

namespace net {

constexpr const std::string_view kSSLDefaultCiphersList = "ALL:!aPSK:!ECDSA+SHA1:!3DES";

// This enum is persisted into histograms. Values may not be renumbered.
enum class SSLHandshakeDetails {
  // TLS 1.2 (or earlier) full handshake (2-RTT)
  kTLS12Full = 0,
  // TLS 1.2 (or earlier) resumption (1-RTT)
  kTLS12Resume = 1,
  // TLS 1.2 full handshake with False Start (1-RTT)
  kTLS12FalseStart = 2,
  // 3 was previously used for TLS 1.3 full handshakes with or without HRR.
  // 4 was previously used for TLS 1.3 resumptions with or without HRR.
  // TLS 1.3 0-RTT handshake (0-RTT)
  kTLS13Early = 5,
  // TLS 1.3 full handshake without HelloRetryRequest (1-RTT)
  kTLS13Full = 6,
  // TLS 1.3 resumption handshake without HelloRetryRequest (1-RTT)
  kTLS13Resume = 7,
  // TLS 1.3 full handshake with HelloRetryRequest (2-RTT)
  kTLS13FullWithHelloRetryRequest = 8,
  // TLS 1.3 resumption handshake with HelloRetryRequest (2-RTT)
  kTLS13ResumeWithHelloRetryRequest = 9,
  kMaxValue = kTLS13ResumeWithHelloRetryRequest,
};

// A OnceCallback specialization that takes a single int parameter. Usually this
// is used to report a byte count or network error code.
using CompletionOnceCallback = absl::AnyInvocable<void(int)>;
using WaitCallback = absl::AnyInvocable<void(asio::error_code ec)>;

class SSLSocket : public RefCountedThreadSafe<SSLSocket> {
 public:
  SSLSocket(int ssl_socket_data_index,
            asio::io_context* io_context,
            asio::ip::tcp::socket* socket,
            SSL_CTX* ssl_ctx,
            bool https_fallback,
            const std::string& host_name);
  ~SSLSocket();

  SSLSocket(SSLSocket&&) = delete;
  SSLSocket& operator=(SSLSocket&&) = delete;

  template <typename... Args>
  static scoped_refptr<SSLSocket> Create(Args&&... args) {
    return MakeRefCounted<SSLSocket>(std::forward<Args>(args)...);
  }

  // StreamSocket implementation
  int Connect(CompletionOnceCallback callback);
  void Disconnect();
  void ConfirmHandshake(CompletionOnceCallback callback);
  int Shutdown(WaitCallback&& cb, bool force = false);

  SSL* native_handle() { return ssl_.get(); }

  // Socket implementation.
  size_t Read(std::shared_ptr<IOBuf> buf, asio::error_code& ec);
  size_t Write(std::shared_ptr<IOBuf> buf, asio::error_code& ec);
  void WaitRead(WaitCallback&& cb);
  void WaitWrite(WaitCallback&& cb);

  NextProto negotiated_protocol() const { return negotiated_protocol_; }

  int NewSessionCallback(SSL_SESSION* session);

 protected:
  void OnWaitRead(asio::error_code ec);
  void OnWaitWrite(asio::error_code ec);
  void OnReadReady();
  void OnWriteReady();
  void OnDoWaitShutdown(asio::error_code ec);

 private:
  int DoHandshake(int* openssl_result);
  int DoHandshakeComplete(int result);
  void DoConnectCallback(int result);

  void OnVerifyComplete(int result);
  void OnHandshakeIOComplete(int result, int openssl_result);
  void RetryAllOperations();

  int DoHandshakeLoop(int last_io_result, int last_openssl_result);
  int DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len);
  int DoPayloadWrite(std::shared_ptr<IOBuf> buf, int buf_len);
  void DoPeek();
  int MapLastOpenSSLError(int ssl_error);

 private:
  int ssl_socket_data_index_;
  asio::io_context* io_context_;
  asio::ip::tcp::socket* stream_socket_;

  CompletionOnceCallback user_connect_callback_;
  WaitCallback wait_read_callback_;
  WaitCallback wait_write_callback_;
  WaitCallback wait_shutdown_callback_;

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

  NextProto negotiated_protocol_ = kProtoUnknown;

  bool IsRenegotiationAllowed() const {
    // Prior to HTTP/2 and SPDY, some servers use TLS renegotiation to request
    // TLS client authentication after the HTTP request was sent. Allow
    // renegotiation for only those connections.
    if (negotiated_protocol_ == kProtoHTTP11) {
      return true;
    }
    // True if renegotiation should be allowed for the default application-level
    // protocol when the peer does not negotiate ALPN.
    bool renego_allowed_default = false;
    return renego_allowed_default;
  }

  // True if SCTs were received via a TLS extension.
  bool signed_cert_timestamps_received_ = false;
  // True if a stapled OCSP response was received.
  bool stapled_ocsp_response_received_ = false;
};

}  // namespace net

#endif  // H_NET_SSL_SOCKET
