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
///
/// The authenticated encryption used in yass program.
///
/// In this protocol, the cipher and MAC are replaced by
/// an Authenticated Encryption with Associated Data (AEAD) algorithm.
/// Several crypto algorithms that implements AEAD algorithms have been
/// included:
///
///    Advanced Encryption Standard (AES) in Galois/Counter
///    Mode [GCM] with 128- and 256-bit keys.
///
///    AEAD_AES_128_GCM
///    AEAD_AES_256_GCM
///    AEAD_AES_128_GCM_12
///    AEAD_AES_192_GCM
///
/// and:
///
///    AEAD_CHACHA20_POLY1305
///    AEAD_XCHACHA20_POLY1305
///
/// Binary Packet Protocol:
///
///    Each packet is in the following format:
///    uint16  packet_length
///    byte[?] authenticated_tag
///    byte[]  payload
///    byte[?] authenticated_tag
///
///    packet_length
///       The length of the packet in bytes, not including 'mac' or the
///       'packet_length' field itself.
///
///    authenticated_tag
///       AEAD Code. If message authentification has been neogotiated,
///       this field contains the AEAD/MAC bytes.
///
///    payload
///       The useful contents of the packet. If compression has been
///       negotiated, this field is compressed. Initially, compression
///       MUST be "none".
///
///  NO padding or MAC is added in this protocol.
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

  bool chunk_decrypt_frame(uint64_t* counter,
                           IOBuf *plaintext, const IOBuf *ciphertext,
                           size_t *consumed_length) const;

  bool chunk_encrypt_frame(uint64_t* counter,
                           const IOBuf *plaintext, IOBuf *ciphertext) const;

  void set_key_aead(const uint8_t *salt, size_t salt_len);

private:
  uint8_t salt_[MAX_KEY_LENGTH];

  uint8_t key_[MAX_KEY_LENGTH];
  uint32_t key_bitlen_;
  uint32_t key_len_;
  uint32_t tag_len_;

  cipher_impl *impl_;
  uint64_t counter_;

  bool init_;
  std::unique_ptr<IOBuf> chunk_;
};

extern enum cipher_method cipher_method_in_use;

#endif // H_CORE_CIPHER
