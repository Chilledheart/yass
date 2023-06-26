// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "net/ssl_socket.hpp"
#include "network.hpp"

namespace net {

namespace {
// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kSSLClientSocketNoPendingResult = 1;
// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kCertVerifyPending = 1;
// Default size of the internal BoringSSL buffers.
const int kDefaultOpenSSLBufferSize = 17 * 1024;
} // namespace

SSLSocket::SSLSocket(asio::io_context *io_context,
                     asio::ip::tcp::socket* socket,
                     SSL_CTX* ssl_ctx,
                     bool https_fallback,
                     const std::string& host_name)
    : io_context_(io_context), stream_socket_(socket),
      early_data_enabled_(absl::GetFlag(FLAGS_tls13_early_data)),
      pending_read_error_(kSSLClientSocketNoPendingResult) {
  DCHECK(!ssl_);
  ssl_.reset(SSL_new(ssl_ctx));

  // TODO: reuse SSL session

  // TODO: implement these SSL options
  // SSLClientSocketImpl::Init
  // SSL_CTX_set_strict_cipher_list
  if (!host_name.empty()) {
    asio::error_code ec;
    asio::ip::make_address(host_name.c_str(), ec);
    bool host_is_ip_address = !ec;
    if (!host_is_ip_address) {
      int ret = SSL_set_tlsext_host_name(ssl_.get(), host_name.c_str());
      CHECK_EQ(ret, 1) << "SSL_set_tlsext_host_name failure";
    }
  }

  SSL_set_early_data_enabled(ssl_.get(), early_data_enabled_);

  // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
  // set everything we care about to an absolute value.
  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);

  // TODO(joth): Set this conditionally, see http://crbug.com/55410
  options.ConfigureFlag(SSL_OP_LEGACY_SERVER_CONNECT, true);

  SSL_set_options(ssl_.get(), options.set_mask);
  SSL_clear_options(ssl_.get(), options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);
  mode.ConfigureFlag(SSL_MODE_CBC_RECORD_SPLITTING, true);

  mode.ConfigureFlag(SSL_MODE_ENABLE_FALSE_START, true);

  SSL_set_mode(ssl_.get(), mode.set_mask);
  SSL_clear_mode(ssl_.get(), mode.clear_mask);

  std::string command("ALL:!aPSK:!ECDSA+SHA1:!3DES");

#if 0
  if (ssl_config_.require_ecdhe) {
    command.append(":!kRSA");
  }

  // Remove any disabled ciphers.
  for (uint16_t id : context_->config().disabled_cipher_suites) {
    const SSL_CIPHER* cipher = SSL_get_cipher_by_value(id);
    if (cipher) {
      command.append(":!");
      command.append(SSL_CIPHER_get_name(cipher));
    }
  }
#endif

  if (!SSL_set_strict_cipher_list(ssl_.get(), command.c_str())) {
    LOG(FATAL) << "SSL_set_cipher_list('" << command << "') failed";
  }

  static const uint16_t kVerifyPrefs[] = {
      SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_RSA_PSS_RSAE_SHA256,
      SSL_SIGN_RSA_PKCS1_SHA256,       SSL_SIGN_ECDSA_SECP384R1_SHA384,
      SSL_SIGN_RSA_PSS_RSAE_SHA384,    SSL_SIGN_RSA_PKCS1_SHA384,
      SSL_SIGN_RSA_PSS_RSAE_SHA512,    SSL_SIGN_RSA_PKCS1_SHA512,
  };
  if (!SSL_set_verify_algorithm_prefs(ssl_.get(), kVerifyPrefs,
                                      std::size(kVerifyPrefs))) {
    LOG(FATAL) << "SSL_set_verify_algorithm_prefs failed";
  }

  // ALPS TLS extension is enabled and corresponding data is sent to client if
  // client also enabled ALPS, for each NextProto in |application_settings|.
  // Data might be empty.
  const char* proto_string = https_fallback ? "http/1.1" : "h2";
  std::vector<uint8_t> data;
  SSL_add_application_settings(ssl_.get(),
                               reinterpret_cast<const uint8_t*>(proto_string),
                               strlen(proto_string), data.data(), data.size());

  SSL_enable_signed_cert_timestamps(ssl_.get());
  SSL_enable_ocsp_stapling(ssl_.get());

  // Configure BoringSSL to allow renegotiations. Once the initial handshake
  // completes, if renegotiations are not allowed, the default reject value will
  // be restored. This is done in this order to permit a BoringSSL
  // optimization. See https://crbug.com/boringssl_.get()/123. Use
  // ssl_.get()_renegotiate_explicit rather than ssl_.get()_renegotiate_freely so DoPeek()
  // does not trigger renegotiations.
  SSL_set_renegotiate_mode(ssl_.get(), ssl_renegotiate_explicit);

  SSL_set_shed_handshake_config(ssl_.get(), 1);

  SSL_set_permute_extensions(ssl_.get(), 1);
}

int SSLSocket::Connect(CompletionOnceCallback callback) {
  // Although StreamSocket does allow calling Connect() after Disconnect(),
  // this has never worked for layered sockets. CHECK to detect any consumers
  // reconnecting an SSL socket.
  //
  // TODO(davidben,mmenke): Remove this API feature. See
  // https://crbug.com/499289.
  CHECK(!disconnected_);

  DCHECK(stream_socket_->native_non_blocking());
  DCHECK(stream_socket_->non_blocking());

  SSL_set_fd(ssl_.get(), stream_socket_->native_handle());

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_.get());

  next_handshake_state_ = STATE_HANDSHAKE;
  int rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = std::move(callback);
  }

  return rv > OK ? OK : rv;
}

SSLSocket::~SSLSocket() {
  VLOG(1) << "SSLSocket " << this << " freed memory";
}

void SSLSocket::RetryAllOperations() {
  // SSL_do_handshake, SSL_read, and SSL_write may all be retried when blocked,
  // so retry all operations for simplicity. (Otherwise, SSL_get_error for each
  // operation may be remembered to retry only the blocked ones.)
  if (disconnected_)
    return;

  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
  }

  if (disconnected_)
    return;

  DoPeek();
}

void SSLSocket::Disconnect() {
  disconnected_ = true;

  // Shut down anything that may call us back.
#if 0
  cert_verifier_request_.reset();
#endif

  // Release user callbacks.
  user_connect_callback_ = nullptr;
  wait_shutdown_callback_ = nullptr;
  wait_read_callback_ = nullptr;
  wait_write_callback_ = nullptr;

  asio::error_code ec;
  stream_socket_->close(ec);
}

// ConfirmHandshake may only be called on a connected socket and, like other
// socket methods, there may only be one ConfirmHandshake operation in progress
// at once.
int SSLSocket::ConfirmHandshake(CompletionOnceCallback callback) {
  CHECK(completed_connect_);
  CHECK(!in_confirm_handshake_);
  if (!SSL_in_early_data(ssl_.get())) {
    callback(OK);
    return OK;
  }

  next_handshake_state_ = STATE_HANDSHAKE;
  in_confirm_handshake_ = true;
  int rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = std::move(callback);
  } else {
    in_confirm_handshake_ = false;
    callback(rv > OK ? OK : rv);
  }

  return rv > OK ? OK : rv;
}

int SSLSocket::Shutdown(WaitCallback callback, bool force) {
  DCHECK(callback);
  DCHECK(!wait_shutdown_callback_ && "Recursively SSL Shutdown isn't allowed");
  if (SSL_in_init(ssl_.get())) {
    callback(asio::error_code());
    return OK;
  }
  if (force) {
    int mode = SSL_RECEIVED_SHUTDOWN | SSL_SENT_SHUTDOWN;
    SSL_set_quiet_shutdown(ssl_.get(), 1);
    SSL_set_shutdown(ssl_.get(), mode);
  }
  ERR_clear_error();
  int tries = 2;
  for (;;) {
    /*
     * For bidirectional shutdown, SSL_shutdown() needs to be called
     * twice: first call sends the "close notify" alert and returns 0,
     * second call waits for the peer's "close notify" alert.
     */
    int result = SSL_shutdown(ssl_.get());
    if (result == 1) {
      callback(asio::error_code());
      return OK;
    }
    if (result == 0 && tries-- > 1)
      continue;
    int sslerr = SSL_get_error(ssl_.get(), result);
    if (sslerr == SSL_ERROR_WANT_READ) {
      scoped_refptr<SSLSocket> self(this);
      VLOG(2) << "Shutdown ... (demand more reading)";

      wait_shutdown_callback_ = [this, self, callback](asio::error_code ec) {
        if (ec) {
          callback(ec);
          return;
        }
        Shutdown(callback);
      };

      if (!wait_read_callback_) {
        stream_socket_->async_wait(asio::ip::tcp::socket::wait_read,
          [self, this](asio::error_code ec){
          OnWaitRead(ec);
        });
      }

      return ERR_IO_PENDING;
    } else if (sslerr == SSL_ERROR_WANT_WRITE) {
      scoped_refptr<SSLSocket> self(this);
      VLOG(2) << "Shutdown ... (demand more writing)";

      wait_shutdown_callback_ = [this, self, callback](asio::error_code ec) {
        if (ec) {
          callback(ec);
          return;
        }
        Shutdown(callback);
      };

      if (!wait_write_callback_) {
        stream_socket_->async_wait(asio::ip::tcp::socket::wait_write,
          [self, this](asio::error_code ec){
          OnWaitWrite(ec);
        });
      }

      return ERR_IO_PENDING;
    }

    if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
      callback(asio::error_code());
      return OK;
    }

    PLOG(WARNING) << "SSL_Shutdown failed with sslerr: " << sslerr;
    callback(asio::error::connection_reset);
    return ERR_UNEXPECTED;
  }
  callback(asio::error_code());
  return OK;
}

size_t SSLSocket::Read(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
  DCHECK(buf->tailroom());
  int buf_len = buf->tailroom();
  int rv = DoPayloadRead(buf, buf_len);
  if (rv == ERR_IO_PENDING) {
    ec = asio::error::try_again;
    return 0;
  }
  if (rv == 0) {
    ec = asio::error::eof;
    return 0;
  } else if (rv < 0) {
    ec = asio::error::connection_refused;
    return 0;
  } else {
#if 0
    buf->append(rv);
#endif
  }
  ec = asio::error_code();
  return rv;
}

size_t SSLSocket::Write(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
  DCHECK(buf->length());

  int rv = DoPayloadWrite(buf, buf->length());

  if (rv == ERR_IO_PENDING) {
    ec = asio::error::try_again;
    return 0;
  } else {
    if (rv > 0)
      was_ever_used_ = true;
  }

  if (rv < 0) {
    ec = asio::error::connection_refused;
    return 0;
  }
  ec = asio::error_code();
  return rv;
}

void SSLSocket::WaitRead(std::function<void(asio::error_code ec)> cb) {
  DCHECK(!wait_read_callback_ && "Multiple calls into Wait Read");
  wait_read_callback_ = cb;
  scoped_refptr<SSLSocket> self(this);
  if (pending_read_error_ != kSSLClientSocketNoPendingResult) {
    OnWaitRead(asio::error_code());
    return;
  }
  if (SSL_pending(ssl_.get())) {
    asio::post(io_context_->get_executor(), [this, self]() {
      OnWaitRead(asio::error_code());
    });
    return;
  }
  stream_socket_->async_wait(asio::ip::tcp::socket::wait_read,
    [this, self](asio::error_code ec){
    OnWaitRead(ec);
  });
}

void SSLSocket::WaitWrite(std::function<void(asio::error_code ec)> cb) {
  DCHECK(!wait_write_callback_ && "Multiple calls into Wait Write");
  wait_write_callback_ = cb;
  scoped_refptr<SSLSocket> self(this);
  stream_socket_->async_wait(asio::ip::tcp::socket::wait_write,
    [this, self](asio::error_code ec){
    OnWaitWrite(ec);
  });
}

void SSLSocket::OnWaitRead(asio::error_code ec) {
  if (disconnected_)
    return;
  if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
    wait_read_callback_ = nullptr;
    wait_write_callback_ = nullptr;
    wait_shutdown_callback_ = nullptr;
    return;
  }
  if (auto cb = std::move(wait_shutdown_callback_)) {
    wait_shutdown_callback_ = nullptr;
    cb(ec);
  }
  if (auto cb = std::move(wait_read_callback_)) {
    wait_read_callback_ = nullptr;
    cb(ec);
  }
}

void SSLSocket::OnWaitWrite(asio::error_code ec) {
  if (disconnected_)
    return;
  if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
    wait_read_callback_ = nullptr;
    wait_write_callback_ = nullptr;
    wait_shutdown_callback_ = nullptr;
    return;
  }
  if (auto cb = std::move(wait_shutdown_callback_)) {
    wait_shutdown_callback_ = nullptr;
    cb(ec);
  }
  if (auto cb = std::move(wait_write_callback_)) {
    wait_write_callback_ = nullptr;
    cb(ec);
  }
}

void SSLSocket::OnReadReady() {
  // During a renegotiation, either Read or Write calls may be blocked on a
  // transport read.
  RetryAllOperations();
}

void SSLSocket::OnWriteReady() {
  // During a renegotiation, either Read or Write calls may be blocked on a
  // transport read.
  RetryAllOperations();
}

int SSLSocket::DoHandshake() {
  int rv = SSL_do_handshake(ssl_.get());
  int net_error = OK;
  if (rv <= 0) {
    int ssl_error = SSL_get_error(ssl_.get(), rv);
    if (ssl_error == SSL_ERROR_WANT_X509_LOOKUP && !send_client_cert_) {
      return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    }
    if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
#if 0
      DCHECK(client_private_key_);
      DCHECK_NE(kSSLClientSocketNoPendingResult, signature_result_);
#endif
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }
    if (ssl_error == SSL_ERROR_WANT_CERTIFICATE_VERIFY) {
#if 0
      DCHECK(cert_verifier_request_);
#endif
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }

    net_error = MapLastOpenSSLError(ssl_error);
    if (net_error == ERR_IO_PENDING) {
      // If not done, stay in this state
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }

    LOG(ERROR) << "handshake failed; returned " << rv << ", SSL error code "
               << ssl_error << ", net_error " << net_error;
  }

  next_handshake_state_ = STATE_HANDSHAKE_COMPLETE;
  return net_error;
}

int SSLSocket::DoHandshakeComplete(int result) {
  if (result < 0)
    return result;

  if (in_confirm_handshake_) {
    next_handshake_state_ = STATE_NONE;
    return OK;
  }

#if 0
  // If ECH overrode certificate verification to authenticate a fallback, using
  // the socket for application data would bypass server authentication.
  // BoringSSL will never complete the handshake in this case, so this should
  // not happen.
  CHECK(!used_ech_name_override_);
#endif

  const uint8_t* alpn_proto = nullptr;
  unsigned alpn_len = 0;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
  if (alpn_len > 0) {
    negotiated_protocol_ = std::string(reinterpret_cast<const char*>(alpn_proto), alpn_len);
  }

#if 0
  RecordNegotiatedProtocol();

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_.get(), &ocsp_response_raw, &ocsp_response_len);
  set_stapled_ocsp_response_received(ocsp_response_len != 0);

  const uint8_t* sct_list;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_.get(), &sct_list, &sct_list_len);
  set_signed_cert_timestamps_received(sct_list_len != 0);

  if (!IsRenegotiationAllowed())
    SSL_set_renegotiate_mode(ssl_.get(), ssl_renegotiate_never);

  uint16_t signature_algorithm = SSL_get_peer_signature_algorithm(ssl_.get());
  if (signature_algorithm != 0) {
    base::UmaHistogramSparse("Net.SSLSignatureAlgorithm", signature_algorithm);
  }

  SSLInfo ssl_info;
  bool ok = GetSSLInfo(&ssl_info);
  // Ensure the verify callback was called, and got far enough to fill
  // in server_cert_.
  CHECK(ok);

  // See how feasible enforcing RSA key usage would be. See
  // https://crbug.com/795089.
  if (!server_cert_verify_result_.is_issued_by_known_root) {
    RSAKeyUsage rsa_key_usage = CheckRSAKeyUsage(
        server_cert_.get(), SSL_get_current_cipher(ssl_.get()));
    if (rsa_key_usage != RSAKeyUsage::kNotRSA) {
      UMA_HISTOGRAM_ENUMERATION("Net.SSLRSAKeyUsage.UnknownRoot", rsa_key_usage,
                                static_cast<int>(RSAKeyUsage::kLastValue) + 1);
    }
  }

  SSLHandshakeDetails details;
  if (SSL_version(ssl_.get()) < TLS1_3_VERSION) {
    if (SSL_session_reused(ssl_.get())) {
      details = SSLHandshakeDetails::kTLS12Resume;
    } else if (SSL_in_false_start(ssl_.get())) {
      details = SSLHandshakeDetails::kTLS12FalseStart;
    } else {
      details = SSLHandshakeDetails::kTLS12Full;
    }
  } else {
    bool used_hello_retry_request = SSL_used_hello_retry_request(ssl_.get());
    if (SSL_in_early_data(ssl_.get())) {
      DCHECK(!used_hello_retry_request);
      details = SSLHandshakeDetails::kTLS13Early;
    } else if (SSL_session_reused(ssl_.get())) {
      details = used_hello_retry_request
                    ? SSLHandshakeDetails::kTLS13ResumeWithHelloRetryRequest
                    : SSLHandshakeDetails::kTLS13Resume;
    } else {
      details = used_hello_retry_request
                    ? SSLHandshakeDetails::kTLS13FullWithHelloRetryRequest
                    : SSLHandshakeDetails::kTLS13Full;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.SSLHandshakeDetails", details);

  // Measure TLS connections that implement the renegotiation_info extension.
  // Note this records true for TLS 1.3. By removing renegotiation altogether,
  // TLS 1.3 is implicitly patched against the bug. See
  // https://crbug.com/850800.
  base::UmaHistogramBoolean("Net.SSLRenegotiationInfoSupported",
                            SSL_get_secure_renegotiation_support(ssl_.get()));
#endif

  completed_connect_ = true;
  next_handshake_state_ = STATE_NONE;

  // Read from the transport immediately after the handshake, whether Read() is
  // called immediately or not. This serves several purposes:
  //
  // First, if this socket is preconnected and negotiates 0-RTT, the ServerHello
  // will not be processed. See https://crbug.com/950706
  //
  // Second, in False Start and TLS 1.3, the tickets arrive after immediately
  // after the handshake. This allows preconnected sockets to process the
  // tickets sooner. This also avoids a theoretical deadlock if the tickets are
  // too large. See
  // https://boringssl-review.googlesource.com/c/boringssl/+/34948.
  //
  // TODO(https://crbug.com/958638): It is also a step in making TLS 1.3 client
  // certificate alerts less unreliable.
  asio::post(*io_context_, [this]() {
    DoPeek();
  });

  return OK;
}

void SSLSocket::DoConnectCallback(int rv) {
  if (auto cb = std::move(user_connect_callback_)) {
    user_connect_callback_ = nullptr;
    cb.operator()(rv > OK ? OK : rv);
  }
}

void SSLSocket::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    if (in_confirm_handshake_) {
      in_confirm_handshake_ = false;
    }
    DoConnectCallback(rv);
  }
}

int SSLSocket::DoHandshakeLoop(int last_io_result) {
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    next_handshake_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_HANDSHAKE_COMPLETE:
        rv = DoHandshakeComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state" << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  if (rv == ERR_IO_PENDING) {
    scoped_refptr<SSLSocket> self(this);
    stream_socket_->async_wait(asio::ip::tcp::socket::wait_read,
      [this, self](asio::error_code ec){
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      OnReadReady();
    });
    stream_socket_->async_wait(asio::ip::tcp::socket::wait_write,
      [this, self](asio::error_code ec){
      if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
        return;
      }
      OnWriteReady();
    });
  }
  return rv;
}

int SSLSocket::DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK(buf);

  int rv;
  if (pending_read_error_ != kSSLClientSocketNoPendingResult) {
    rv = pending_read_error_;
    pending_read_error_ = kSSLClientSocketNoPendingResult;
#if 0
    if (rv == 0) {
      net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_RECEIVED,
                                    rv, buf->data());
    } else {
      NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, rv,
                         pending_read_ssl_error_, pending_read_error_info_);
    }
#endif
    pending_read_ssl_error_ = SSL_ERROR_NONE;
    return rv;
  }

  int total_bytes_read = 0;
  int ssl_ret, ssl_err;
  do {
    ssl_ret = SSL_read(ssl_.get(), buf->mutable_tail() + total_bytes_read,
                       buf_len - total_bytes_read);
    ssl_err = SSL_get_error(ssl_.get(), ssl_ret);
    if (ssl_ret > 0) {
      total_bytes_read += ssl_ret;
    } else if (ssl_err == SSL_ERROR_WANT_RENEGOTIATE) {
      if (!SSL_renegotiate(ssl_.get())) {
        ssl_err = SSL_ERROR_SSL;
      }
    }
    // Continue processing records as long as there is more data available
    // synchronously.
  } while (ssl_err == SSL_ERROR_WANT_RENEGOTIATE ||
           (total_bytes_read < buf_len && ssl_ret > 0));

  // Although only the final SSL_read call may have failed, the failure needs to
  // processed immediately, while the information still available in OpenSSL's
  // error queue.
  if (ssl_ret <= 0) {
    pending_read_ssl_error_ = ssl_err;
    if (pending_read_ssl_error_ == SSL_ERROR_ZERO_RETURN) {
      pending_read_error_ = 0;
    } else if (pending_read_ssl_error_ == SSL_ERROR_WANT_X509_LOOKUP &&
               !send_client_cert_) {
      pending_read_error_ = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    } else if (pending_read_ssl_error_ ==
               SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
#if 0
      DCHECK(client_private_key_);
      DCHECK_NE(kSSLClientSocketNoPendingResult, signature_result_);
#endif
      pending_read_error_ = ERR_IO_PENDING;
    } else {
      pending_read_error_ = MapLastOpenSSLError(pending_read_ssl_error_);
    }

    // Many servers do not reliably send a close_notify alert when shutting down
    // a connection, and instead terminate the TCP connection. This is reported
    // as ERR_CONNECTION_CLOSED. Because of this, map the unclean shutdown to a
    // graceful EOF, instead of treating it as an error as it should be.
    if (pending_read_error_ == ERR_CONNECTION_CLOSED)
      pending_read_error_ = 0;
  }

  if (total_bytes_read > 0) {
    // Return any bytes read to the caller. The error will be deferred to the
    // next call of DoPayloadRead.
    rv = total_bytes_read;

    // Do not treat insufficient data as an error to return in the next call to
    // DoPayloadRead() - instead, let the call fall through to check SSL_read()
    // again. The transport may have data available by then.
    if (pending_read_error_ == ERR_IO_PENDING)
      pending_read_error_ = kSSLClientSocketNoPendingResult;
  } else {
    // No bytes were returned. Return the pending read error immediately.
    DCHECK_NE(kSSLClientSocketNoPendingResult, pending_read_error_);
    rv = pending_read_error_;
    pending_read_error_ = kSSLClientSocketNoPendingResult;
  }

  if (rv >= 0) {
#if 0
    net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_RECEIVED,
                                  rv, buf->data());
#endif
  } else if (rv != ERR_IO_PENDING) {
#if 0
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, rv,
                       pending_read_ssl_error_, pending_read_error_info_);
#endif
    pending_read_ssl_error_ = SSL_ERROR_NONE;
  }
  return rv;
}

int SSLSocket::DoPayloadWrite(std::shared_ptr<IOBuf> buf, int buf_len) {
  int rv = SSL_write(ssl_.get(), buf->data(), buf_len);

  if (rv >= 0) {
    if (first_post_handshake_write_ && SSL_is_init_finished(ssl_.get())) {
#if 0
      if (base::FeatureList::IsEnabled(features::kTLS13KeyUpdate) &&
          SSL_version(ssl_.get()) == TLS1_3_VERSION) {
        const int ok = SSL_key_update(ssl_.get(), SSL_KEY_UPDATE_REQUESTED);
        DCHECK(ok);
      }
#endif
      first_post_handshake_write_ = false;
    }
    return rv;
  }

  int ssl_error = SSL_get_error(ssl_.get(), rv);
  if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION)
    return ERR_IO_PENDING;
  int net_error = MapLastOpenSSLError(ssl_error);

  if (net_error != ERR_IO_PENDING) {
#if 0
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_WRITE_ERROR, net_error,
                       ssl_error, error_info);
#endif
  }
  return net_error;
}

void SSLSocket::DoPeek() {
  if (!completed_connect_) {
    return;
  }

  if (early_data_enabled_ && !handled_early_data_result_) {
    // |SSL_peek| will implicitly run |SSL_do_handshake| if needed, but run it
    // manually to pick up the reject reason.
    int rv = SSL_do_handshake(ssl_.get());
    int ssl_err = SSL_get_error(ssl_.get(), rv);
    int err = rv > 0 ? OK : MapOpenSSLError(ssl_err);
    if (err == ERR_IO_PENDING) {
      return;
    }

    // On early data reject, clear early data on any other sessions in the
    // cache, so retries do not get stuck attempting 0-RTT. See
    // https://crbug.com/1066623.
    if (err == ERR_EARLY_DATA_REJECTED ||
        err == ERR_WRONG_VERSION_ON_EARLY_DATA) {
      LOG(WARNING) << "Early data rejected";
#if 0
      context_->ssl_client_session_cache()->ClearEarlyData(
          GetSessionCacheKey(absl::nullopt));
#endif
    }

    handled_early_data_result_ = true;

    if (err != OK) {
      peek_complete_ = true;
      return;
    }
  }

  if (peek_complete_) {
    return;
  }

  char byte;
  int rv = SSL_peek(ssl_.get(), &byte, 1);
  int ssl_err = SSL_get_error(ssl_.get(), rv);
  if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
    peek_complete_ = true;
  }
}

int SSLSocket::MapLastOpenSSLError(
    int ssl_error) {
  int net_error = MapOpenSSLErrorWithDetails(ssl_error);

#if 0
  if (ssl_error == SSL_ERROR_SSL &&
      ERR_GET_LIB(info->error_code) == ERR_LIB_SSL) {
    // TLS does not provide an alert for missing client certificates, so most
    // servers send a generic handshake_failure alert. Detect this case by
    // checking if we have received a CertificateRequest but sent no
    // certificate. See https://crbug.com/646567.
    if (ERR_GET_REASON(info->error_code) ==
            SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE &&
        certificate_requested_ && send_client_cert_ && !client_cert_) {
      net_error = ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }

    // Per spec, access_denied is only for client-certificate-based access
    // control, but some buggy firewalls use it when blocking a page. To avoid a
    // confusing error, map it to a generic protocol error if no
    // CertificateRequest was sent. See https://crbug.com/630883.
    if (ERR_GET_REASON(info->error_code) == SSL_R_TLSV1_ALERT_ACCESS_DENIED &&
        !certificate_requested_) {
      net_error = ERR_SSL_PROTOCOL_ERROR;
    }

    // This error is specific to the client, so map it here.
    if (ERR_GET_REASON(info->error_code) ==
        SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS) {
      net_error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS;
    }
  }
#endif

  return net_error;
}

} // namespace net
