// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef H_CRYPTO_CRYPTER
#define H_CRYPTO_CRYPTER

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace crypto {

class Crypter {
 public:
  virtual ~Crypter();

  // Sets the symmetric encryption/decryption key. Returns true on success,
  // false on failure.
  //
  // NOTE: The key is the client_write_key or server_write_key derived from
  // the master secret.
  virtual bool SetKey(const char* key, size_t key_len) = 0;

  // Sets the fixed initial bytes of the nonce. Returns true on success,
  // false on failure. This method must only be used with Google QUIC crypters.
  //
  // NOTE: The nonce prefix is the client_write_iv or server_write_iv
  // derived from the master secret. A 64-bit packet number will
  // be appended to form the nonce.
  //
  //                          <------------ 64 bits ----------->
  //   +---------------------+----------------------------------+
  //   |    Fixed prefix     |      packet number      |
  //   +---------------------+----------------------------------+
  //                          Nonce format
  //
  // The security of the nonce format requires that QUIC never reuse a
  // packet number, even when retransmitting a lost packet.
  virtual bool SetNoncePrefix(const char* nonce_prefix,
                              size_t nonce_prefix_len) = 0;

  // Sets |iv| as the initialization vector to use when constructing the nonce.
  // Returns true on success, false on failure. This method must only be used
  // with IETF QUIC crypters.
  //
  // Google QUIC and IETF QUIC use different nonce constructions. This method
  // must be used when using IETF QUIC; SetNoncePrefix must be used when using
  // Google QUIC.
  //
  // The nonce is constructed as follows (draft-ietf-quic-tls-14 section 5.2):
  //
  //    <---------------- max(8, N_MIN) bytes ----------------->
  //   +--------------------------------------------------------+
  //   |                 packet protection IV                   |
  //   +--------------------------------------------------------+
  //                             XOR
  //                          <------------ 64 bits ----------->
  //   +---------------------+----------------------------------+
  //   |        zeroes       |   reconstructed packet number    |
  //   +---------------------+----------------------------------+
  //
  // The nonce is the packet protection IV (|iv|) XOR'd with the left-padded
  // reconstructed packet number.
  //
  // The security of the nonce format requires that QUIC never reuse a
  // packet number, even when retransmitting a lost packet.
  virtual bool SetIV(const char* iv, size_t iv_len) = 0;

#if 0
  // Sets the key to use for header protection.
  virtual bool SetHeaderProtectionKey(const char* key, size_t key_len) = 0;
#endif

  // GetKeySize, GetIVSize, and GetNoncePrefixSize are used to know how many
  // bytes of key material needs to be derived from the master secret.

  // Returns the size in bytes of a key for the algorithm.
  virtual size_t GetKeySize() const = 0;
  // Returns the size in bytes of an IV to use with the algorithm.
  virtual size_t GetIVSize() const = 0;
  // Returns the size in bytes of the fixed initial part of the nonce.
  virtual size_t GetNoncePrefixSize() const = 0;
  // Returns the size in bytes of the auth tag size(AEAD).
  virtual size_t GetTagSize() const = 0;
};

}  // namespace crypto

#endif  // H_CRYPTO_CRYPTER
