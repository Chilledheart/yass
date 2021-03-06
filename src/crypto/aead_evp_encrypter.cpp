// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "crypto/aead_evp_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/crypto.h>
#include <openssl/err.h>

// In debug builds only, log OpenSSL error stack. Then clear OpenSSL error
// stack.
static void DLogOpenSslErrors() {
#ifdef NDEBUG
  while (ERR_get_error()) {
  }
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

EvpAeadEncrypter::EvpAeadEncrypter(const EVP_AEAD *(*aead_getter)(),
                                   size_t key_size, size_t auth_tag_size,
                                   size_t nonce_size)
    : AeadBaseEncrypter(key_size, auth_tag_size, nonce_size),
      aead_alg_(InitAndCall(aead_getter)) {}

EvpAeadEncrypter::~EvpAeadEncrypter() {}

bool EvpAeadEncrypter::SetKey(const char *key, size_t key_len) {
  if (!AeadBaseEncrypter::SetKey(key, key_len)) {
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

bool EvpAeadEncrypter::Encrypt(const char *nonce, size_t nonce_len,
                               const char *associated_data,
                               size_t associated_data_len,
                               const char *plaintext, size_t plaintext_len,
                               uint8_t *output) {
  DCHECK_EQ(nonce_len, nonce_size_);

  size_t ciphertext_len;
  if (!EVP_AEAD_CTX_seal(
          ctx_.get(), output, &ciphertext_len, plaintext_len + auth_tag_size_,
          reinterpret_cast<const uint8_t *>(nonce), nonce_len,
          reinterpret_cast<const uint8_t *>(plaintext), plaintext_len,
          reinterpret_cast<const uint8_t *>(associated_data),
          associated_data_len)) {
    DLogOpenSslErrors();
    return false;
  }

  return true;
}

bool EvpAeadEncrypter::EncryptPacket(
    uint64_t packet_number, const char *associated_data,
    size_t associated_data_len, const char *plaintext, size_t plaintext_len,
    char *output, size_t *output_length, size_t max_output_length) {
  size_t ciphertext_size = GetCiphertextSize(plaintext_len);
  if (max_output_length < ciphertext_size) {
    return false;
  }
  char nonce_buffer[kMaxNonceSize];
  memcpy(nonce_buffer, iv_, nonce_size_);
  size_t prefix_len = nonce_size_ - sizeof(uint64_t);
  memcpy(nonce_buffer + prefix_len, &packet_number, sizeof(uint64_t));

  if (!Encrypt((const char *)nonce_buffer, nonce_size_, associated_data,
               associated_data_len, plaintext, plaintext_len,
               (uint8_t *)output)) {
    return false;
  }
  *output_length = ciphertext_size;
  return true;
}

} // namespace crypto

#endif // HAVE_BORINGSSL
