// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "crypto/aes_256_gcm_sodium_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "core/cipher.hpp"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

Aes256GcmSodiumEncrypter::Aes256GcmSodiumEncrypter()
    : SodiumAeadEncrypter(EVP_aead_aes_256_gcm,
                          kKeySize,
                          kAuthTagSize,
                          kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes256GcmSodiumEncrypter::~Aes256GcmSodiumEncrypter() = default;

uint32_t Aes256GcmSodiumEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256;
}

}  // namespace crypto

#endif
