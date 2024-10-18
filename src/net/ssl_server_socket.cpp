// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023-2024 Chilledheart  */

#include "net/ssl_server_socket.hpp"

#include "config/config_tls.hpp"
#include "net/openssl_util.hpp"
#include "third_party/boringssl/src/include/openssl/err.h"

#define GotoState(s) next_handshake_state_ = s

namespace net {

namespace {
// Default size of the internal BoringSSL buffers.
const int kDefaultOpenSSLBufferSize = 17 * 1024;
}  // namespace

SSLServerSocket::SSLServerSocket(asio::io_context* io_context, asio::ip::tcp::socket* socket, SSL_CTX* ssl_ctx)
    : io_context_(io_context), stream_socket_(socket) {
  DCHECK(!ssl_);
  DCHECK(ssl_ctx);
  ssl_.reset(SSL_new(ssl_ctx));

  // TODO: SSL_set_app_data
  // TODO: reuse SSL session

  SSL_set_shed_handshake_config(ssl_.get(), 1);
  // TODO:
  // Set certificate and private key.
  // SSL_set_signing_algorithm_prefs
  if (TEST_post_quantumn_only_mode && absl::GetFlag(FLAGS_enable_post_quantum_kyber)) {
    const uint16_t postquantum_group =
        absl::GetFlag(FLAGS_use_ml_kem) ? SSL_GROUP_X25519_MLKEM768 : SSL_GROUP_X25519_KYBER768_DRAFT00;
    const uint16_t kGroups[] = {postquantum_group};
    int ret = SSL_set1_group_ids(ssl_.get(), kGroups, std::size(kGroups));
    CHECK_EQ(ret, 1) << "SSL_set1_group_ids failure";
  } else if (absl::GetFlag(FLAGS_enable_post_quantum_kyber)) {
    const uint16_t postquantum_group =
        absl::GetFlag(FLAGS_use_ml_kem) ? SSL_GROUP_X25519_MLKEM768 : SSL_GROUP_X25519_KYBER768_DRAFT00;
    const uint16_t kGroups[] = {postquantum_group, SSL_GROUP_X25519, SSL_GROUP_SECP256R1, SSL_GROUP_SECP384R1};
    int ret = SSL_set1_group_ids(ssl_.get(), kGroups, std::size(kGroups));
    CHECK_EQ(ret, 1) << "SSL_set1_group_ids failure";
  }
}

SSLServerSocket::~SSLServerSocket() {
  VLOG(1) << "SSLServerSocket " << this << " freed memory";
  if (ssl_) {
    ssl_.reset();
  }
}

int SSLServerSocket::Handshake(CompletionOnceCallback callback) {
  CHECK(!disconnected_);

  DCHECK(stream_socket_->non_blocking());

  SSL_set_fd(ssl_.get(), stream_socket_->native_handle());

  // Set SSL to server mode. Handshake happens in the loop below.
  SSL_set_accept_state(ssl_.get());

  GotoState(STATE_HANDSHAKE);
  int rv = DoHandshakeLoop(OK, SSL_ERROR_NONE);
  if (rv == ERR_IO_PENDING) {
    user_handshake_callback_ = std::move(callback);
  } else {
#if 0
    net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE,
                                      rv);
#endif
  }

  return rv > OK ? OK : rv;
}

int SSLServerSocket::Shutdown(WaitCallback&& callback, bool force) {
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
      scoped_refptr<SSLServerSocket> self(this);
      VLOG(2) << "Shutdown ... (demand more reading)";

      wait_shutdown_callback_ = std::move(callback);

      if (!wait_read_callback_) {
        stream_socket_->async_wait(asio::ip::tcp::socket::wait_read,
                                   [self, this](asio::error_code ec) { OnWaitRead(ec); });
      }

      return ERR_IO_PENDING;
    } else if (sslerr == SSL_ERROR_WANT_WRITE) {
      scoped_refptr<SSLServerSocket> self(this);
      VLOG(2) << "Shutdown ... (demand more writing)";

      wait_shutdown_callback_ = std::move(callback);

      if (!wait_write_callback_) {
        stream_socket_->async_wait(asio::ip::tcp::socket::wait_write,
                                   [self, this](asio::error_code ec) { OnWaitWrite(ec); });
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

void SSLServerSocket::Disconnect() {
  disconnected_ = true;

  // Release user callbacks.
  wait_shutdown_callback_ = nullptr;
  wait_read_callback_ = nullptr;
  wait_write_callback_ = nullptr;

  asio::error_code ec;
  stream_socket_->close(ec);
}

size_t SSLServerSocket::Read(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
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

size_t SSLServerSocket::Write(std::shared_ptr<IOBuf> buf, asio::error_code& ec) {
  DCHECK(buf->length());

  int rv = DoPayloadWrite(buf, buf->length());

  if (rv == ERR_IO_PENDING) {
    ec = asio::error::try_again;
    return 0;
  }

  if (rv < 0) {
    ec = asio::error::connection_refused;
    return 0;
  }
  ec = asio::error_code();
  return rv;
}

void SSLServerSocket::WaitRead(WaitCallback&& cb) {
  DCHECK(!wait_read_callback_ && "Multiple calls into Wait Read");
  wait_read_callback_ = std::move(cb);
  scoped_refptr<SSLServerSocket> self(this);
#if 0
  if (SSL_pending(ssl_.get())) {
    asio::post(io_context_->get_executor(), [this, self]() {
      OnWaitRead(asio::error_code());
    });
    return;
  }
#endif
  stream_socket_->async_wait(asio::ip::tcp::socket::wait_read, [this, self](asio::error_code ec) { OnWaitRead(ec); });
}

void SSLServerSocket::WaitWrite(WaitCallback&& cb) {
  DCHECK(!wait_write_callback_ && "Multiple calls into Wait Write");
  wait_write_callback_ = std::move(cb);
  scoped_refptr<SSLServerSocket> self(this);
  stream_socket_->async_wait(asio::ip::tcp::socket::wait_write, [this, self](asio::error_code ec) { OnWaitWrite(ec); });
}

void SSLServerSocket::OnWaitRead(asio::error_code ec) {
  if (disconnected_)
    return;
  if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
    wait_read_callback_ = nullptr;
    wait_write_callback_ = nullptr;
    wait_shutdown_callback_ = nullptr;
    return;
  }
  if (wait_shutdown_callback_) {
    OnDoWaitShutdown(ec);
  }
  if (auto cb = std::move(wait_read_callback_)) {
    wait_read_callback_ = nullptr;
    cb(ec);
  }
}

void SSLServerSocket::OnWaitWrite(asio::error_code ec) {
  if (disconnected_)
    return;
  if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
    wait_read_callback_ = nullptr;
    wait_write_callback_ = nullptr;
    wait_shutdown_callback_ = nullptr;
    return;
  }
  if (wait_shutdown_callback_) {
    OnDoWaitShutdown(ec);
  }
  if (auto cb = std::move(wait_write_callback_)) {
    wait_write_callback_ = nullptr;
    cb(ec);
  }
}

void SSLServerSocket::OnReadReady() {
  if (disconnected_)
    return;
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK, SSL_ERROR_NONE);
    return;
  }
}

void SSLServerSocket::OnWriteReady() {
  if (disconnected_)
    return;
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK, SSL_ERROR_NONE);
    return;
  }
}

void SSLServerSocket::OnDoWaitShutdown(asio::error_code ec) {
  DCHECK(wait_shutdown_callback_);
  auto callback = std::move(wait_shutdown_callback_);
  DCHECK(!wait_shutdown_callback_);
  if (ec) {
    callback(ec);
    return;
  }
  Shutdown(std::move(callback));
}

void SSLServerSocket::OnHandshakeIOComplete(int result, int openssl_result) {
  int rv = DoHandshakeLoop(result, openssl_result);
  if (rv == ERR_IO_PENDING)
    return;

#if 0
  net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE, rv);
#endif
  if (user_handshake_callback_)
    DoHandshakeCallback(rv);
}

int SSLServerSocket::DoHandshake(int* openssl_result) {
  int net_error = OK;
  int rv = SSL_do_handshake(ssl_.get());
  *openssl_result = SSL_ERROR_NONE;
  if (rv == 1) {
    const uint8_t* alpn_proto = nullptr;
    unsigned alpn_len = 0;
    SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
    if (alpn_len > 0) {
      std::string_view proto(reinterpret_cast<const char*>(alpn_proto), alpn_len);
      negotiated_protocol_ = NextProtoFromString(proto);
    }
#if 0
    const STACK_OF(CRYPTO_BUFFER)* certs =
        SSL_get0_peer_certificates(ssl_.get());
    if (certs) {
      client_cert_ = x509_util::CreateX509CertificateFromBuffers(certs);
      if (!client_cert_)
        return ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT;
    }

    if (context_->ssl_server_config_.alert_after_handshake_for_testing) {
      SSL_send_fatal_alert(ssl_.get(),
                           context_->ssl_server_config_
                               .alert_after_handshake_for_testing.value());
      return ERR_FAILED;
    }
#endif

    completed_handshake_ = true;
  } else {
    int ssl_error = SSL_get_error(ssl_.get(), rv);
    *openssl_result = ssl_error;

    if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
#if 0
      DCHECK(context_->private_key_);
#endif
      GotoState(STATE_HANDSHAKE);
      return ERR_IO_PENDING;
    }

    net_error = MapOpenSSLErrorWithDetails(ssl_error);

#if 0
    // SSL_R_CERTIFICATE_VERIFY_FAILED's mapping is different between client and
    // server.
    if (ERR_GET_LIB(error_info.error_code) == ERR_LIB_SSL &&
        ERR_GET_REASON(error_info.error_code) ==
            SSL_R_CERTIFICATE_VERIFY_FAILED) {
      net_error = ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }
#endif

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; returned " << rv << ", SSL error code " << ssl_error << ", net_error "
                 << net_error;
#if 0
      NetLogOpenSSLError(net_log_, NetLogEventType::SSL_HANDSHAKE_ERROR,
                         net_error, ssl_error, error_info);
#endif
    }
  }
  return net_error;
}

void SSLServerSocket::DoHandshakeCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  std::move(user_handshake_callback_).operator()(rv > OK ? OK : rv);
  user_handshake_callback_ = nullptr;
}

int SSLServerSocket::DoHandshakeLoop(int last_io_result, int last_sslerr) {
  int rv = last_io_result;
  int sslerr = last_sslerr;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake(&sslerr);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  if (rv == ERR_IO_PENDING) {
    scoped_refptr<SSLServerSocket> self(this);
    if (sslerr == SSL_ERROR_WANT_READ) {
      stream_socket_->async_wait(asio::ip::tcp::socket::wait_read, [this, self](asio::error_code ec) {
        if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
          return;
        }
        OnReadReady();
      });
    } else if (sslerr == SSL_ERROR_WANT_WRITE) {
      stream_socket_->async_wait(asio::ip::tcp::socket::wait_write, [this, self](asio::error_code ec) {
        if (ec == asio::error::bad_descriptor || ec == asio::error::operation_aborted) {
          return;
        }
        OnWriteReady();
      });
    } else {
      DLOG(FATAL) << "ERR_IO_PENDING without next sslerr: " << sslerr;
    }
  }
  return rv;
}

int SSLServerSocket::DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  int rv = SSL_read(ssl_.get(), buf->mutable_tail(), buf_len);
  if (rv >= 0) {
    if (SSL_in_early_data(ssl_.get()))
      early_data_received_ = true;
    return rv;
  }
  int ssl_error = SSL_get_error(ssl_.get(), rv);
  int net_error = MapOpenSSLErrorWithDetails(ssl_error);
  if (net_error != ERR_IO_PENDING) {
#if 0
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, net_error,
                       ssl_error, error_info);
#endif
  }
  return net_error;
}

int SSLServerSocket::DoPayloadWrite(std::shared_ptr<IOBuf> buf, int buf_len) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);

  int rv = SSL_write(ssl_.get(), buf->data(), buf_len);
  if (rv >= 0)
    return rv;
  int ssl_error = SSL_get_error(ssl_.get(), rv);
  int net_error = MapOpenSSLErrorWithDetails(ssl_error);
  if (net_error != ERR_IO_PENDING) {
#if 0
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_WRITE_ERROR, net_error,
                       ssl_error, error_info);
#endif
  }
  return net_error;
}

int SSLServerSocket::MapLastOpenSSLError(int ssl_error) {
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

bool SSLServerSocket::TEST_post_quantumn_only_mode = false;

}  // namespace net
