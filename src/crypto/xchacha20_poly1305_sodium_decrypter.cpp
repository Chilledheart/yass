// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "crypto/xchacha20_poly1305_sodium_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include "crypto/crypter_export.hpp"
#include "third_party/boringssl/src/include/openssl/aead.h"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 24;

namespace crypto {

XChaCha20Poly1305SodiumDecrypter::XChaCha20Poly1305SodiumDecrypter()
    : AeadSodiumDecrypter(EVP_aead_xchacha20_poly1305, kKeySize, kAuthTagSize, kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

XChaCha20Poly1305SodiumDecrypter::~XChaCha20Poly1305SodiumDecrypter() = default;

uint32_t XChaCha20Poly1305SodiumDecrypter::cipher_id() const {
  return CRYPTO_XCHACHA20POLY1305IETF;
}

}  // namespace crypto

#endif
