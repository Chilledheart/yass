//
// aes_256_gcm_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/aes_256_gcm_evp_encrypter.hpp"

#include "core/logging.hpp"

#ifdef HAVE_BORINGSSL
#include "core/cipher.hpp"
#include <openssl/aead.h>

static const size_t kKeySize = 32;
static const size_t kNonceSize = 12;

namespace crypto {

Aes256GcmEvpEncrypter::Aes256GcmEvpEncrypter()
    : EvpAeadEncrypter(EVP_aead_aes_256_gcm, kKeySize, kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes256GcmEvpEncrypter::~Aes256GcmEvpEncrypter() {}

uint32_t Aes256GcmEvpEncrypter::cipher_id() const {
  return CRYPTO_AES256GCMSHA256_EVP;
}

} // namespace crypto

#endif
