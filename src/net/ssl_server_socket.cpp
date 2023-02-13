// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "net/ssl_server_socket.hpp"

#include "net/openssl_util.hpp"
#include <openssl/err.h>

#define GotoState(s) next_handshake_state_ = s

namespace net {

namespace {
// Default size of the internal BoringSSL buffers.
const int kDefaultOpenSSLBufferSize = 17 * 1024;
} // namespace

SSLServerSocket::SSLServerSocket(asio::io_context *io_context,
                                 asio::ip::tcp::socket* socket,
                                 SSL_CTX* ssl_ctx)
    : io_context_(io_context), stream_socket_(socket) {
  DCHECK(!ssl_);
  ssl_.reset(SSL_new(ssl_ctx));

  // TODO: reuse SSL session

  transport_adapter_ = std::make_unique<SocketBIOAdapter>(
      io_context, socket, kDefaultOpenSSLBufferSize,
      kDefaultOpenSSLBufferSize, this);
  BIO* transport_bio = transport_adapter_->bio();

  BIO_up_ref(transport_bio);  // SSL_set0_rbio takes ownership.
  SSL_set0_rbio(ssl_.get(), transport_bio);

  BIO_up_ref(transport_bio);  // SSL_set0_wbio takes ownership.
  SSL_set0_wbio(ssl_.get(), transport_bio);

  SSL_set_shed_handshake_config(ssl_.get(), 1);
}

SSLServerSocket::~SSLServerSocket() {
  if (ssl_) {
    SSL_shutdown(ssl_.get());
    ssl_.reset();
  }
}

int SSLServerSocket::Handshake(CompletionOnceCallback callback) {
  // Set SSL to server mode. Handshake happens in the loop below.
  SSL_set_accept_state(ssl_.get());

  GotoState(STATE_HANDSHAKE);
  int rv = DoHandshakeLoop(OK);
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

void SSLServerSocket::Disconnect() {
  asio::error_code ec;
  stream_socket_->close(ec);
}

size_t SSLServerSocket::Read(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
  int buf_len = buf->capacity();
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

size_t SSLServerSocket::Write(std::shared_ptr<IOBuf> buf, asio::error_code &ec) {
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

void SSLServerSocket::OnReadReady() {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
    return;
  }
}

void SSLServerSocket::OnWriteReady() {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
    return;
  }
}

void SSLServerSocket::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv == ERR_IO_PENDING)
    return;

#if 0
  net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE, rv);
#endif
  if (user_handshake_callback_)
    DoHandshakeCallback(rv);
}

int SSLServerSocket::DoHandshake() {
  int net_error = OK;
  int rv = SSL_do_handshake(ssl_.get());
  if (rv == 1) {
#if 0
    const STACK_OF(CRYPTO_BUFFER)* certs =
        SSL_get0_peer_certificates(ssl_.get());
    if (certs) {
      client_cert_ = x509_util::CreateX509CertificateFromBuffers(certs);
      if (!client_cert_)
        return ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT;
    }

    const uint8_t* alpn_proto = nullptr;
    unsigned alpn_len = 0;
    SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
    if (alpn_len > 0) {
      base::StringPiece proto(reinterpret_cast<const char*>(alpn_proto),
                              alpn_len);
      negotiated_protocol_ = NextProtoFromString(proto);
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
      LOG(ERROR) << "handshake failed; returned " << rv << ", SSL error code "
                 << ssl_error << ", net_error " << net_error;
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
}

int SSLServerSocket::DoHandshakeLoop(int last_io_result) {
  int rv = last_io_result;
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
        rv = DoHandshake();
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLServerSocket::DoPayloadRead(std::shared_ptr<IOBuf> buf, int buf_len) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  int rv = SSL_read(ssl_.get(), buf->mutable_data(), buf_len);
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

int SSLServerSocket::MapLastOpenSSLError(
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
