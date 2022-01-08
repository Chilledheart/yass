// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_AES128_GCM12_EVP_ENCRYPTER
#define H_CRYPTO_AES128_GCM12_EVP_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_encrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class Aes128Gcm12EvpEncrypter : public EvpAeadEncrypter {
 public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  Aes128Gcm12EvpEncrypter();
  ~Aes128Gcm12EvpEncrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_AES128_GCM12_EVP_ENCRYPTER
