// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER
#define H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER

#ifdef HAVE_LIBSODIUM

#include "aead_base_encrypter.hpp"

namespace crypto {

class ChaCha20Poly1305SodiumEncrypter : public AeadBaseEncrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  ChaCha20Poly1305SodiumEncrypter();
  ~ChaCha20Poly1305SodiumEncrypter() override;

  bool EncryptPacket(uint64_t packet_number, const char *associated_data,
                     size_t associated_data_len, const char *plaintext,
                     size_t plaintext_len, char *output, size_t *output_length,
                     size_t max_output_length) override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif // HAVE_LIBSODIUM

#endif // H_CRYPTO_CHACHA20_POLY1305_SODIUM_ENCRYPTER
