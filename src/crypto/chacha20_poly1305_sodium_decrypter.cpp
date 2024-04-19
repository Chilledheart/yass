// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#include "crypto/chacha20_poly1305_sodium_decrypter.hpp"

#include "core/logging.hpp"

#include "crypto/crypter_export.hpp"
#include "third_party/boringssl/src/include/openssl/aead.h"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

ChaCha20Poly1305SodiumDecrypter::ChaCha20Poly1305SodiumDecrypter()
    : AeadSodiumDecrypter(EVP_aead_chacha20_poly1305, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

ChaCha20Poly1305SodiumDecrypter::~ChaCha20Poly1305SodiumDecrypter() = default;

uint32_t ChaCha20Poly1305SodiumDecrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF;
}

}  // namespace crypto
