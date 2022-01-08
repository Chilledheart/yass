// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/aes_128_gcm_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "core/cipher.hpp"

static const size_t kKeySize = 16;
static const size_t kNonceSize = 12;

namespace crypto {

Aes128GcmEvpDecrypter::Aes128GcmEvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_128_gcm,
                       kKeySize,
                       kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes128GcmEvpDecrypter::~Aes128GcmEvpDecrypter() {}

uint32_t Aes128GcmEvpDecrypter::cipher_id() const {
  return CRYPTO_AES128GCMSHA256_EVP;
}

}  // namespace crypto

#endif
