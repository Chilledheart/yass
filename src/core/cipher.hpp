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

#include <cstdint>
#include <functional>
#include <sodium/utils.h>
#include <string>

#include "iobuf.hpp"
#include "protocol.hpp"

#define MAX_KEY_LENGTH 64
#define MAX_NONCE_LENGTH 32

class cipher {
public:
  enum cipher_method {
    PLAINTEXT = 0x0,
    SALSA20 = 0x1,
    CHACHA20 = 0x2,
    CHACHA20IETF = 0x3,
    CHACHA20POLY1305IETF = 0x4,
    XCHACHA20POLY1305IETF = 0x5,
    MAX_CIPHER_METHOD,
  };

  static cipher_method to_cipher_method(const std::string &method) {
    if (method == "salsa20") {
      return SALSA20;
    } else if (method == "chacha20") {
      return CHACHA20;
    } else if (method == "chacha20-ietf") {
      return CHACHA20IETF;
    } else if (method == "chacha20-ietf-poly1305") {
      return CHACHA20POLY1305IETF;
    } else if (method == "xchacha20-ietf-poly1305") {
      return XCHACHA20POLY1305IETF;
    }
    return PLAINTEXT;
  }
  static const char *to_cipher_method_str(cipher_method method) {
    switch (method) {
    case SALSA20:
      return "salsa20";
    case CHACHA20:
      return "chacha20";
    case CHACHA20IETF:
      return "chacha20-ietf";
    case CHACHA20POLY1305IETF:
      return "chacha20-ietf-poly1305";
    case XCHACHA20POLY1305IETF:
      return "xchacha20-ietf-poly1305";
    default:
      return "plaintext";
    }
  }

  cipher(const std::string &key, const std::string &password,
         cipher_method method, bool enc = false);

  void decrypt(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
               size_t capacity = SOCKET_BUF_SIZE) {
    if (method_ >= CHACHA20POLY1305IETF) {
      decrypt_aead(ciphertext, plaintext, capacity);
    } else {
      decrypt_stream(ciphertext, plaintext, capacity);
    }
  }

  void encrypt(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
               size_t capacity = SOCKET_BUF_SIZE) {
    if (method_ >= CHACHA20POLY1305IETF) {
      encrypt_aead(plaintext, ciphertext, capacity);
    } else {
      encrypt_stream(plaintext, ciphertext, capacity);
    }
  }

  void decrypt_stream(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
                      size_t capacity);

  void encrypt_stream(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
                      size_t capacity);

  void set_key_stream();

  void decrypt_aead(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
                    size_t capacity);

  void encrypt_aead(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
                    size_t capacity);

  bool chunk_decrypt_aead(IOBuf *plaintext, const IOBuf *ciphertext,
                          size_t *consumed_length) const;

  bool chunk_encrypt_aead(const IOBuf *plaintext, IOBuf *ciphertext) const;

  void set_key_aead();

private:
private:
  uint8_t key_[MAX_KEY_LENGTH];
  const uint8_t method_;
  const uint32_t key_bitlen_;
  const uint32_t key_len_;
  const uint32_t nonce_len_;
  const uint32_t tag_len_;

  bool init_;
  uint32_t nonce_left_;

  uint8_t skey_[MAX_KEY_LENGTH];
  mutable uint8_t nonce_[MAX_NONCE_LENGTH];

  uint8_t salt_[MAX_KEY_LENGTH];
  std::unique_ptr<IOBuf> chunk_;

  uint64_t counter_;
};

extern cipher::cipher_method cipher_method;

#endif // H_CORE_CIPHER
