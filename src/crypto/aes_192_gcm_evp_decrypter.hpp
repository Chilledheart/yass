// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_AES192_GCM_EVP_DECRYPTER
#define H_CRYPTO_AES192_GCM_EVP_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_decrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class Aes192GcmEvpDecrypter : public AeadEvpDecrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes192GcmEvpDecrypter();
  ~Aes192GcmEvpDecrypter() override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_AES192_GCM_EVP_DECRYPTER
