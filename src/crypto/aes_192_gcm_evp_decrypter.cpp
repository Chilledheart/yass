// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "crypto/aes_192_gcm_evp_decrypter.hpp"

#include "core/logging.hpp"

#include "crypto/crypter_export.hpp"
#include "third_party/boringssl/src/include/openssl/aead.h"

static const size_t kKeySize = 24;
static const size_t kNonceSize = 12;

namespace crypto {

Aes192GcmEvpDecrypter::Aes192GcmEvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_192_gcm, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes192GcmEvpDecrypter::~Aes192GcmEvpDecrypter() = default;

uint32_t Aes192GcmEvpDecrypter::cipher_id() const {
  return CRYPTO_AES192GCMSHA256_EVP;
}

}  // namespace crypto
