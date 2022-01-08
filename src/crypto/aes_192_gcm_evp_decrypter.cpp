// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/aes_192_gcm_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "core/cipher.hpp"

static const size_t kKeySize = 24;
static const size_t kNonceSize = 12;

namespace crypto {

Aes192GcmEvpDecrypter::Aes192GcmEvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_192_gcm,
                       kKeySize,
                       kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes192GcmEvpDecrypter::~Aes192GcmEvpDecrypter() {}

uint32_t Aes192GcmEvpDecrypter::cipher_id() const {
  return CRYPTO_AES192GCMSHA256_EVP;
}

}  // namespace crypto

#endif
