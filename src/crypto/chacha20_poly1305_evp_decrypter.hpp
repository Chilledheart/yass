// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_CHACHA20_POLY1305_EVP_DECRYPTER
#define H_CRYPTO_CHACHA20_POLY1305_EVP_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "crypto/aead_evp_decrypter.hpp"

#ifdef HAVE_BORINGSSL

namespace crypto {

class ChaCha20Poly1305EvpDecrypter : public AeadEvpDecrypter {
public:
  enum : size_t {
    kAuthTagSize = 12,
  };

  ChaCha20Poly1305EvpDecrypter();
  ~ChaCha20Poly1305EvpDecrypter() override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_CHACHA20_POLY1305_EVP_DECRYPTER
