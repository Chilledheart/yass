// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/aead_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/crypto.h>
#include <openssl/err.h>

// Clear OpenSSL error stack.
static void ClearOpenSslErrors() {
  while (ERR_get_error()) {
  }
}

// In debug builds only, log OpenSSL error stack. Then clear OpenSSL error
// stack.
static void DLogOpenSslErrors() {
#ifdef NDEBUG
  ClearOpenSslErrors();
#else
  while (uint32_t error = ERR_get_error()) {
    char buf[120];
    ERR_error_string_n(error, buf, sizeof(buf));
    DLOG(ERROR) << "OpenSSL error: " << buf;
  }
#endif
}

static const EVP_AEAD *InitAndCall(const EVP_AEAD *(*aead_getter)()) {
  // Ensure BoringSSL is initialized before calling |aead_getter|. In Chromium,
  // the static initializer is disabled.
  ::CRYPTO_library_init();
  return aead_getter();
}

namespace crypto {

AeadEvpDecrypter::AeadEvpDecrypter(const EVP_AEAD *(*aead_getter)(),
                                   size_t key_size, size_t auth_tag_size,
                                   size_t nonce_size)
    : AeadBaseDecrypter(key_size, auth_tag_size, nonce_size),
      aead_alg_(InitAndCall(aead_getter)) {}

AeadEvpDecrypter::~AeadEvpDecrypter() {}

bool AeadEvpDecrypter::SetKey(const char *key, size_t key_len) {
  if (!AeadBaseDecrypter::SetKey(key, key_len)) {
    return false;
  }
  EVP_AEAD_CTX_cleanup(ctx_.get());
  if (!EVP_AEAD_CTX_init(ctx_.get(), aead_alg_, key_, key_size_, auth_tag_size_,
                         nullptr)) {
    DLogOpenSslErrors();
    return false;
  }
  return true;
}

bool AeadEvpDecrypter::DecryptPacket(
    uint64_t packet_number, const char *associated_data,
    size_t associated_data_len, const char *ciphertext, size_t ciphertext_len,
    char *output, size_t *output_length, size_t max_output_length) {
  if (ciphertext_len < auth_tag_size_) {
    return false;
  }

  if (have_preliminary_key_) {
    LOG(ERROR) << "Unable to decrypt while key diversification is pending";
    return false;
  }

  uint8_t nonce[kMaxNonceSize];
  memcpy(nonce, iv_, nonce_size_);
  size_t prefix_len = nonce_size_ - sizeof(packet_number);
  memcpy(nonce + prefix_len, &packet_number, sizeof(packet_number));
  if (!EVP_AEAD_CTX_open(
          ctx_.get(), reinterpret_cast<uint8_t *>(output), output_length,
          max_output_length, reinterpret_cast<const uint8_t *>(nonce),
          nonce_size_, reinterpret_cast<const uint8_t *>(ciphertext),
          ciphertext_len, reinterpret_cast<const uint8_t *>(associated_data),
          associated_data_len)) {
    ClearOpenSslErrors();
    return false;
  }
  return true;
}

} // namespace crypto

#endif // HAVE_BORINGSSL
