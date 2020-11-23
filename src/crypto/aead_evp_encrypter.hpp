//
// aead_evp_encrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_AEAD_EVP_ENCRYPTER
#define H_CRYPTO_AEAD_EVP_ENCRYPTER

#include "aead_base_encrypter.hpp"

#ifdef HAVE_BORINGSSL

#include <openssl/aead.h>

namespace crypto {

class EvpAeadEncrypter : public AeadBaseEncrypter {
public:
  EvpAeadEncrypter(const EVP_AEAD *(*aead_getter)(), size_t key_size,
                   size_t auth_tag_size, size_t nonce_size);
  ~EvpAeadEncrypter() override;

  bool SetKey(const char *key, size_t key_len) override;

  bool EncryptPacket(uint64_t packet_number, const char *associated_data,
                     size_t associated_data_len, const char *plaintext,
                     size_t plaintext_len, char *output, size_t *output_length,
                     size_t max_output_length) override;

  bool Encrypt(const char *nonce, size_t nonce_len, const char *associated_data,
               size_t associated_data_len, const char *plaintext,
               size_t plaintext_len, uint8_t *output);

private:
  const EVP_AEAD *const aead_alg_;
  bssl::ScopedEVP_AEAD_CTX ctx_;
};

} // namespace crypto

#endif

#endif // H_CRYPTO_AEAD_EVP_ENCRYPTER
