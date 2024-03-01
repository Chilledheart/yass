// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "net/openssl_util.hpp"
#include "net/net_errors.hpp"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "core/logging.hpp"

namespace net {

SslSetClearMask::SslSetClearMask() = default;

void SslSetClearMask::ConfigureFlag(long flag, bool state) {
  (state ? set_mask : clear_mask) |= flag;
  // Make sure we haven't got any intersection in the set & clear options.
  DCHECK_EQ(0, set_mask & clear_mask) << flag << ":" << state;
}

namespace {

class OpenSSLNetErrorLibSingleton {
 public:
  OpenSSLNetErrorLibSingleton() {
    // CRYPTO_library_init may be safely called concurrently.
    CRYPTO_library_init();
    // Allocate a new error library value for inserting net errors into
    // OpenSSL. This does not register any ERR_STRING_DATA for the errors, so
    // stringifying error codes through OpenSSL will return NULL.
    net_error_lib_ = ERR_get_next_error_library();
  }

  int net_error_lib() const { return net_error_lib_; }

 private:
  int net_error_lib_;
};

}  // namespace

int OpenSSLNetErrorLib() {
  static OpenSSLNetErrorLibSingleton g_openssl_net_error_lib = {};
  return g_openssl_net_error_lib.net_error_lib();
}

void OpenSSLPutNetError(const char* file, int line, int err) {
  // Net error codes are negative. Encode them as positive numbers.
  err = -err;
  if (err < 0 || err > 0xfff) {
    // OpenSSL reserves 12 bits for the reason code.
    NOTREACHED();
    err = ERR_INVALID_ARGUMENT;
  }
  ERR_put_error(OpenSSLNetErrorLib(), 0 /* unused */, err, file, line);
}

int MapOpenSSLErrorSSL(uint32_t error_code) {
  DCHECK_EQ(ERR_LIB_SSL, ERR_GET_LIB(error_code));

#if DCHECK_IS_ON()
  char buf[ERR_ERROR_STRING_BUF_LEN];
  ERR_error_string_n(error_code, buf, sizeof(buf));
  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code) << ", name: " << buf;
#endif

  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_R_TLSV1_UNRECOGNIZED_NAME:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_R_SERVER_CERT_CHANGED:
      return ERR_SSL_SERVER_CERT_CHANGED;
    case SSL_R_WRONG_VERSION_ON_EARLY_DATA:
      return ERR_WRONG_VERSION_ON_EARLY_DATA;
    case SSL_R_TLS13_DOWNGRADE:
      return ERR_TLS13_DOWNGRADE_DETECTED;
    case SSL_R_ECH_REJECTED:
      return ERR_ECH_NOT_NEGOTIATED;
    // SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the server after
    // receiving ClientHello if there's no common supported cipher. Map that
    // specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH to match the NSS
    // implementation. See https://goo.gl/oMtZW and https://crbug.com/446505.
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE: {
      uint32_t previous = ERR_peek_error();
      if (previous != 0 && ERR_GET_LIB(previous) == ERR_LIB_SSL &&
          ERR_GET_REASON(previous) == SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO) {
        return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
      }
      return ERR_SSL_PROTOCOL_ERROR;
    }
    case SSL_R_KEY_USAGE_BIT_INCORRECT:
      return ERR_SSL_KEY_USAGE_INCOMPATIBLE;
    default:
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

int MapOpenSSLErrorWithDetails(int err) {
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_EARLY_DATA_REJECTED:
      return ERR_EARLY_DATA_REJECTED;
    case SSL_ERROR_SYSCALL:
      PLOG(ERROR) << "OpenSSL SYSCALL error, earliest error code in "
                     "error queue: "
                  << ERR_peek_error();
      return ERR_FAILED;
    case SSL_ERROR_SSL:
      // Walk down the error stack to find an SSL or net error.
      while (true) {
        const char* file;
        int line;
        int error_code = ERR_get_error_line(&file, &line);
        if (error_code == 0) {
          // Map errors to ERR_SSL_PROTOCOL_ERROR by default, reporting the most
          // recent error in |*out_error_info|.
          return ERR_SSL_PROTOCOL_ERROR;
        }

        if (ERR_GET_LIB(error_code) == ERR_LIB_SSL) {
          return MapOpenSSLErrorSSL(error_code);
        }
        if (ERR_GET_LIB(error_code) == OpenSSLNetErrorLib()) {
          // Net error codes are negative but encoded in OpenSSL as positive
          // numbers.
          return -ERR_GET_REASON(error_code);
        }
      }
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

}  // namespace net
