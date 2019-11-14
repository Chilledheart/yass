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

#include "crypto/decrypter.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace crypto {

class AeadBaseDecrypter : public Decrypter {
public:
  AeadBaseDecrypter(size_t key_size, size_t auth_tag_size, size_t nonce_size);
  virtual ~AeadBaseDecrypter();

  bool SetKey(const char* key, size_t key_len) override;
  bool SetNoncePrefix(const char* nonce_prefix, size_t nonce_prefix_len) override;
  bool SetIV(const char* iv, size_t iv_len) override;
  bool SetPreliminaryKey(const char* key, size_t key_len) override;

  size_t GetKeySize() const override;
  size_t GetNoncePrefixSize() const override;
  size_t GetIVSize() const override;
  size_t GetTagSize() const override;

  const uint8_t *GetKey() const override;
  const uint8_t *GetNoncePrefix() const override;

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
