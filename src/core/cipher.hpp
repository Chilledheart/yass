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

#include "iobuf.hpp"
#include "protocol.hpp"

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32

enum cipher_method {
  PLAINTEXT = 0x0,
  /* UNUSED = 0x1 */
  /* UNUSED = 0x2 */
  /* UNUSED = 0x3 */
  CHACHA20POLY1305IETF = 0x4,
  XCHACHA20POLY1305IETF = 0x5,
#ifdef HAVE_BORINGSSL
  CHACHA20POLY1305IETF_EVP = 0x6,
  XCHACHA20POLY1305IETF_EVP = 0x7,
#endif
  MAX_CIPHER_METHOD,
};

enum cipher_method to_cipher_method(const std::string &method);
const char *to_cipher_method_str(enum cipher_method method);

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
