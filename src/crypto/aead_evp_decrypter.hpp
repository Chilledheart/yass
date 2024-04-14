// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_AEAD_EVP_DECRYPTER
#define H_CRYPTO_AEAD_EVP_DECRYPTER

#include "aead_base_decrypter.hpp"

#ifdef HAVE_BORINGSSL

#include "third_party/boringssl/src/include/openssl/aead.h"

namespace crypto {

class AeadEvpDecrypter : public AeadBaseDecrypter {
 public:
  AeadEvpDecrypter(const EVP_AEAD* (*aead_getter)(), size_t key_size, size_t auth_tag_size, size_t nonce_size);
  ~AeadEvpDecrypter() override;

  bool SetKey(const char* key, size_t key_len) override;

  bool DecryptPacket(uint64_t packet_number,
                     const char* associated_data,
                     size_t associated_data_len,
                     const char* ciphertext,
                     size_t ciphertext_len,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;

 private:
  const EVP_AEAD* const aead_alg_;
  bssl::ScopedEVP_AEAD_CTX ctx_;
};

}  // namespace crypto

#endif

#endif  // H_CRYPTO_AEAD_EVP_DECRYPTER
