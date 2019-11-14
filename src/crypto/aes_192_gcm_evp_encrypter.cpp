//
// aes_192_gcm_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/aes_192_gcm_evp_encrypter.hpp"

#include <glog/logging.h>

#ifdef HAVE_BORINGSSL
#include "core/cipher.hpp"
#include <openssl/aead.h>

static const size_t kKeySize = 24;
static const size_t kNonceSize = 12;

namespace crypto {

Aes192GcmEvpEncrypter::Aes192GcmEvpEncrypter()
    : EvpAeadEncrypter(EVP_aead_aes_192_gcm, kKeySize, kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes192GcmEvpEncrypter::~Aes192GcmEvpEncrypter() {}

uint32_t Aes192GcmEvpEncrypter::cipher_id() const {
  return CRYPTO_AES192GCMSHA256_EVP;
}

} // namespace crypto

#endif
