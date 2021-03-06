// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER
#define H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER

#ifdef HAVE_LIBSODIUM

#include "aead_base_encrypter.hpp"

extern "C" typedef struct crypto_aead_aes256gcm_state_
    crypto_aead_aes256gcm_state;

namespace crypto {

class Aes256GcmSodiumEncrypter : public AeadBaseEncrypter {
public:
  enum : size_t {
    kAuthTagSize = 16,
  };

  Aes256GcmSodiumEncrypter();
  ~Aes256GcmSodiumEncrypter() override;

  bool SetKey(const char *key, size_t key_len) override;

  bool EncryptPacket(uint64_t packet_number, const char *associated_data,
                     size_t associated_data_len, const char *plaintext,
                     size_t plaintext_len, char *output, size_t *output_length,
                     size_t max_output_length) override;

  uint32_t cipher_id() const override;

private:
  crypto_aead_aes256gcm_state *ctx_;
};

} // namespace crypto

#endif // HAVE_LIBSODIUM

#endif // H_CRYPTO_AES256_GCM_SODIUM_ENCRYPTER
