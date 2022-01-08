// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_XCHACHA20_POLY1305_EVP_ENCRYPTER
#define H_CRYPTO_XCHACHA20_POLY1305_EVP_ENCRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_encrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class XChaCha20Poly1305EvpEncrypter : public EvpAeadEncrypter {
 public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  XChaCha20Poly1305EvpEncrypter();
  ~XChaCha20Poly1305EvpEncrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_XCHACHA20_POLY1305_EVP_ENCRYPTER
