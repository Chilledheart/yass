//
// aead_base_decrypter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CRYPTO_AEAD_BASE_DECRYPTER
#define H_CRYPTO_AEAD_BASE_DECRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace crypto {

class AeadBaseDecrypter {
public:
  AeadBaseDecrypter(size_t key_size, size_t auth_tag_size, size_t nonce_size);
  virtual ~AeadBaseDecrypter();

  virtual bool SetKey(const char* key, size_t key_len);
  virtual bool SetNoncePrefix(const char* nonce_prefix, size_t nonce_prefix_len);
  virtual bool SetIV(const char* iv, size_t iv_len);
  virtual bool SetPreliminaryKey(const char* key, size_t key_len);
  virtual bool DecryptPacket(uint64_t packet_number,
                             const char *associated_data,
                             size_t associated_data_len,
                             const char *ciphertext,
                             size_t ciphertext_len,
                             char *output,
                             size_t *output_length,
                             size_t max_output_length) = 0;

  size_t GetKeySize() const;
  size_t GetNoncePrefixSize() const;
  size_t GetIVSize() const;
  size_t GetTagSize() const;

  const uint8_t *GetKey() const;
  const uint8_t *GetNoncePrefix() const;

  virtual uint32_t cipher_id() const = 0;

protected:
  static const size_t kMaxKeySize = 64;
  enum : size_t { kMaxNonceSize = 32 };

  const size_t key_size_;
  const size_t auth_tag_size_;
  const size_t nonce_size_;
  bool have_preliminary_key_;

  // The Key.
  uint8_t key_[kMaxKeySize];
  // The IV used to construct the nonce.
  uint8_t iv_[kMaxNonceSize];
};

} // namespace crypto

#endif // H_CRYPTO_AEAD_BASE_DECRYPTER
