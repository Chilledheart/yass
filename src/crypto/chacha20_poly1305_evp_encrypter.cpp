//
// chacha20_poly1305_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/chacha20_poly1305_evp_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include "core/cipher.hpp"
#include <openssl/aead.h>

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

ChaCha20Poly1305EvpEncrypter::ChaCha20Poly1305EvpEncrypter()
    : EvpAeadEncrypter(EVP_aead_chacha20_poly1305, kKeySize, kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

ChaCha20Poly1305EvpEncrypter::~ChaCha20Poly1305EvpEncrypter() {}

uint32_t ChaCha20Poly1305EvpEncrypter::cipher_id() const {
  return CRYPTO_CHACHA20POLY1305IETF_EVP;
}

} // namespace crypto

#endif
