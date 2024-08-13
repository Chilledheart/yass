// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#include "crypto/aead_evp_encrypter.hpp"

#include "core/logging.hpp"
#include "net/protocol.hpp"

#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/err.h"

// In debug builds only, log OpenSSL error stack. Then clear OpenSSL error
// stack.
static void DLogOpenSslErrors() {
#ifdef NDEBUG
  ERR_clear_error();
#else
  while (uint32_t error = ERR_get_error()) {
    char buf[120];
    ERR_error_string_n(error, buf, sizeof(buf));
    DLOG(ERROR) << "OpenSSL error: " << buf;
  }
#endif
}

namespace crypto {

EvpAeadEncrypter::EvpAeadEncrypter(const EVP_AEAD* (*aead_getter)(),
                                   size_t key_size,
                                   size_t auth_tag_size,
                                   size_t nonce_size)
    : AeadBaseEncrypter(key_size, auth_tag_size, nonce_size), aead_alg_(aead_getter()) {
  DCHECK_EQ(EVP_AEAD_key_length(aead_alg_), key_size);
  DCHECK_EQ(EVP_AEAD_nonce_length(aead_alg_), nonce_size);
  DCHECK_GE(EVP_AEAD_max_tag_len(aead_alg_), auth_tag_size);
}

EvpAeadEncrypter::~EvpAeadEncrypter() = default;

bool EvpAeadEncrypter::SetKey(const char* key, size_t key_len) {
  if (!AeadBaseEncrypter::SetKey(key, key_len)) {
    return false;
  }
  EVP_AEAD_CTX_cleanup(ctx_.get());
  if (!EVP_AEAD_CTX_init(ctx_.get(), aead_alg_, key_, key_size_, auth_tag_size_, nullptr)) {
    DLogOpenSslErrors();
    return false;
  }
  return true;
}

bool EvpAeadEncrypter::Encrypt(const uint8_t* nonce,
                               size_t nonce_len,
                               const char* associated_data,
                               size_t associated_data_len,
                               const char* plaintext,
                               size_t plaintext_len,
                               uint8_t* output,
                               size_t* output_length,
                               size_t max_output_length) {
  DCHECK_EQ(nonce_len, nonce_size_);

  if (!EVP_AEAD_CTX_seal(ctx_.get(), output, output_length, max_output_length, nonce, nonce_len,
                         reinterpret_cast<const uint8_t*>(plaintext), plaintext_len,
                         reinterpret_cast<const uint8_t*>(associated_data), associated_data_len)) {
    DLogOpenSslErrors();
    return false;
  }

  return true;
}

bool EvpAeadEncrypter::EncryptPacket(uint64_t packet_number,
                                     const char* associated_data,
                                     size_t associated_data_len,
                                     const char* plaintext,
                                     size_t plaintext_len,
                                     char* output,
                                     size_t* output_length,
                                     size_t max_output_length) {
  size_t ciphertext_size = GetCiphertextSize(plaintext_len);
  if (max_output_length < ciphertext_size) {
    return false;
  }
  *output_length = max_output_length;

  uint8_t nonce[kMaxNonceSize] = {};
  memcpy(nonce, iv_, nonce_size_);
  PacketNumberToNonceEvp(nonce, nonce_size_, packet_number);

  DumpHex("EN-NONCE", nonce, nonce_size_);

  if (!Encrypt(nonce, nonce_size_, associated_data, associated_data_len, plaintext, plaintext_len,
               reinterpret_cast<uint8_t*>(output), output_length, max_output_length)) {
    return false;
  }
  DCHECK_EQ(*output_length, ciphertext_size);
  return true;
}

}  // namespace crypto
