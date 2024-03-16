// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "crypto/chacha20_poly1305_sodium_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "crypto/crypter_export.hpp"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

ChaCha20Poly1305SodiumEncrypter::ChaCha20Poly1305SodiumEncrypter()
    : SodiumAeadEncrypter(EVP_aead_chacha20_poly1305, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

ChaCha20Poly1305SodiumEncrypter::~ChaCha20Poly1305SodiumEncrypter() = default;

uint32_t ChaCha20Poly1305SodiumEncrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF;
}

}  // namespace crypto

#endif
