// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#ifndef H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER
#define H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_decrypter.hpp"

namespace crypto {

class XChaCha20Poly1305EvpDecrypter : public AeadEvpDecrypter {
 public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  XChaCha20Poly1305EvpDecrypter();
  ~XChaCha20Poly1305EvpDecrypter() override;

  uint32_t cipher_id() const override;
};

}  // namespace crypto

#endif  // H_CRYPTO_XCHACHA20_POLY1305_EVP_DECRYPTER
