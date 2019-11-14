//
// cipher.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef H_CORE_CIPHER
#define H_CORE_CIPHER

#include <memory>
#include <stdint.h>
#include <string>
#include <utility>

#include "core/iobuf.hpp"
#include "protocol.hpp"
#include "crypto/crypter_export.hpp"

class cipher_impl;
class cipher {
public:
  cipher(const std::string &key, const std::string &password,
         enum cipher_method method, bool enc = false);
  ~cipher();

  void decrypt(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
               size_t capacity = SOCKET_BUF_SIZE);

  void encrypt(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
               size_t capacity = SOCKET_BUF_SIZE);

private:
  void decrypt_salt(IOBuf *chunk);

  void encrypt_salt(IOBuf *chunk);

  bool chunk_decrypt_aead(IOBuf *plaintext, const IOBuf *ciphertext,
                          size_t *consumed_length);

  bool chunk_encrypt_aead(const IOBuf *plaintext, IOBuf *ciphertext);

  void set_key_aead(const uint8_t *salt, size_t salt_len);

private:
  uint8_t salt_[MAX_KEY_LENGTH];

  uint8_t key_[MAX_KEY_LENGTH];
  uint32_t key_bitlen_;
  uint32_t key_len_;
  uint32_t tag_len_;
  uint8_t nonce_[MAX_NONCE_LENGTH];

  cipher_impl *impl_;
  uint64_t counter_;

  bool init_;
  std::unique_ptr<IOBuf> chunk_;
};

extern enum cipher_method cipher_method;

#endif // H_CORE_CIPHER
