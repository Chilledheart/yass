// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/aes_256_gcm_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include "core/cipher.hpp"
#include <openssl/aead.h>

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

Aes256GcmEvpDecrypter::Aes256GcmEvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_256_gcm, kKeySize, kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes256GcmEvpDecrypter::~Aes256GcmEvpDecrypter() {}

uint32_t Aes256GcmEvpDecrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256_EVP;
}

} // namespace crypto

#endif
