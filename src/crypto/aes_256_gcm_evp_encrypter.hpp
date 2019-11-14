//
// aes_256_gcm_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_AES256_GCM_EVP_ENCRYPTER
#define H_CRYPTO_AES256_GCM_EVP_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_encrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class Aes256GcmEvpEncrypter : public EvpAeadEncrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmEvpEncrypter();
  ~Aes256GcmEvpEncrypter() override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_AES256_GCM_EVP_ENCRYPTER
