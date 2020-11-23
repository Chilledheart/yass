//
// aes_256_gcm_sodium_decrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER
#define H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER

#include "aead_base_decrypter.hpp"

#ifdef HAVE_LIBSODIUM

extern "C" typedef struct crypto_aead_aes256gcm_state_
    crypto_aead_aes256gcm_state;

namespace crypto {

class Aes256GcmSodiumDecrypter : public AeadBaseDecrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmSodiumDecrypter();
  ~Aes256GcmSodiumDecrypter() override;

  bool SetKey(const char *key, size_t key_len) override;

  bool DecryptPacket(uint64_t packet_number, const char *associated_data,
                     size_t associated_data_len, const char *ciphertext,
                     size_t ciphertext_len, char *output, size_t *output_length,
                     size_t max_output_length) override;

  uint32_t cipher_id() const override;

private:
  crypto_aead_aes256gcm_state *ctx_;
};

} // namespace crypto

#endif // HAVE_LIBSODIUM

#endif // H_CRYPTO_AES256_GCM_SODIUM_DECRYPTER
