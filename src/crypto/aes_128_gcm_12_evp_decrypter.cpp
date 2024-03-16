// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/aes_128_gcm_12_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "crypto/crypter_export.hpp"

static const size_t kKeySize = 16;
static const size_t kNonceSize = 12;

namespace crypto {

Aes128Gcm12EvpDecrypter::Aes128Gcm12EvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_128_gcm, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes128Gcm12EvpDecrypter::~Aes128Gcm12EvpDecrypter() = default;

uint32_t Aes128Gcm12EvpDecrypter::cipher_id() const {
  return CRYPTO_AES128GCM12SHA256_EVP;
}

}  // namespace crypto

#endif
