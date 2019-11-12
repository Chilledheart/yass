//
// chacha20_poly1305_sodium_decrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_CHACHA20_POLY1305_SODIUM_DECRYPTER
#define H_CRYPTO_CHACHA20_POLY1305_SODIUM_DECRYPTER

#include "aead_base_decrypter.hpp"

namespace crypto {

class ChaCha20Poly1305SodiumDecrypter : public AeadBaseDecrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  ChaCha20Poly1305SodiumDecrypter();
  ~ChaCha20Poly1305SodiumDecrypter() override;

  bool DecryptPacket(uint64_t packet_number,
                     const char *associated_data,
                     size_t associated_data_len,
                     const char *ciphertext,
                     size_t ciphertext_len,
                     char *output,
                     size_t *output_length,
                     size_t max_output_length) override;

  uint32_t cipher_id() const override;
};

} // namespace crypto

#endif // H_CRYPTO_CHACHA20_POLY1305_SODIUM_DECRYPTER
