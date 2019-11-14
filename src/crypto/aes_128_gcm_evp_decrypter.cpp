//
// aes_128_gcm_evp_decrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "crypto/aes_128_gcm_evp_decrypter.hpp"

#include <glog/logging.h>

#ifdef HAVE_BORINGSSL
#include "core/cipher.hpp"
#include <openssl/aead.h>

static const size_t kKeySize = 16;
static const size_t kNonceSize = 12;

namespace crypto {

Aes128GcmEvpDecrypter::Aes128GcmEvpDecrypter()
    : AeadEvpDecrypter(EVP_aead_aes_128_gcm, kKeySize, kAuthTagSize,
                       kNonceSize) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

Aes128GcmEvpDecrypter::~Aes128GcmEvpDecrypter() {}

uint32_t Aes128GcmEvpDecrypter::cipher_id() const {
  return AES128GCMSHA256_EVP;
}

} // namespace crypto

#endif
