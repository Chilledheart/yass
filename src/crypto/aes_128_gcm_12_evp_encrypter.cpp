// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "crypto/aes_128_gcm_12_evp_encrypter.hpp"

#include "core/logging.hpp"

#include "crypto/crypter_export.hpp"
#include "third_party/boringssl/src/include/openssl/aead.h"

static const size_t kKeySize = 16;
static const size_t kNonceSize = 12;

namespace crypto {

Aes128Gcm12EvpEncrypter::Aes128Gcm12EvpEncrypter()
    : EvpAeadEncrypter(EVP_aead_aes_128_gcm, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes128Gcm12EvpEncrypter::~Aes128Gcm12EvpEncrypter() = default;

uint32_t Aes128Gcm12EvpEncrypter::cipher_id() const {
  return CRYPTO_AES128GCM12SHA256_EVP;
}

}  // namespace crypto
