// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "crypto/xchacha20_poly1305_evp_decrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include <openssl/aead.h>
#include "core/cipher.hpp"

static const size_t kKeySize = 32;
static const size_t kNonceSize = 24;

namespace crypto {

XChaCha20Poly1305EvpDecrypter::XChaCha20Poly1305EvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_xchacha20_poly1305,
                       kKeySize,
                       kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

XChaCha20Poly1305EvpDecrypter::~XChaCha20Poly1305EvpDecrypter() = default;

uint32_t XChaCha20Poly1305EvpDecrypter::cipher_id() const {
  return CRYPTO_XCHACHA20POLY1305IETF_EVP;
}

}  // namespace crypto

#endif
