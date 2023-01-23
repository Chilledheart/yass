// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CORE_CIPHER
#define H_CORE_CIPHER

#include <memory>
#include <stdint.h>
#include <string>
#include <utility>

#ifdef _WIN32
#include <malloc.h>
#endif

#include "core/iobuf.hpp"
#include "crypto/crypter_export.hpp"
#include "protocol.hpp"

class cipher_visitor_interface {
 public:
  virtual bool on_received_data(std::shared_ptr<IOBuf> buf) = 0;
  virtual void on_protocol_error() = 0;
};
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
  cipher(const std::string& key,
         const std::string& password,
         enum cipher_method method,
         cipher_visitor_interface *visitor,
         bool enc = false);
  ~cipher();

  void process_bytes(std::shared_ptr<IOBuf> ciphertext);

  void encrypt(IOBuf* plaintext,
               std::shared_ptr<IOBuf>* ciphertext);

 private:
  void decrypt_salt(IOBuf* chunk);

  void encrypt_salt(IOBuf* chunk);

  int chunk_decrypt_frame(uint64_t* counter,
                          IOBuf* plaintext,
                          IOBuf* ciphertext) const;

  int chunk_encrypt_frame(uint64_t* counter,
                          const IOBuf* plaintext,
                          IOBuf* ciphertext) const;

  void set_key_aead(const uint8_t* salt, size_t salt_len);

 private:
  uint8_t salt_[MAX_KEY_LENGTH];

  uint8_t key_[MAX_KEY_LENGTH];
  uint32_t key_bitlen_;
  uint32_t key_len_;
  uint32_t tag_len_;

  std::unique_ptr<cipher_impl> impl_;
  uint64_t counter_;

  bool init_;
  std::unique_ptr<IOBuf> chunk_;

  cipher_visitor_interface *visitor_;
};

#endif  // H_CORE_CIPHER
