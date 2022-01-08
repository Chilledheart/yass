// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/aes_256_gcm_evp_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "core/cipher.hpp"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

Aes256GcmEvpEncrypter::Aes256GcmEvpEncrypter()
    : EvpAeadEncrypter(EVP_aead_aes_256_gcm,
                       kKeySize,
                       kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes256GcmEvpEncrypter::~Aes256GcmEvpEncrypter() {}

uint32_t Aes256GcmEvpEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256_EVP;
}

}  // namespace crypto

#endif
