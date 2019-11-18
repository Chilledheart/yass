//
// cipher.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/cipher.hpp"

#include <memory>

#include "core/hkdf_sha1.hpp"
#include "core/md5.h"
#include "core/modp_b64.h"
#include "core/sys_byteorder.h"
#include "core/rand_util.hpp"
#include "crypto/encrypter.hpp"
#include "crypto/decrypter.hpp"

#define CHUNK_SIZE_LEN 2U
#define CHUNK_SIZE_MASK 0x3FFFU

#define MD_MAX_SIZE_256 32 /* longest known is SHA256 or less */

class cipher_impl {
public:
  cipher_impl(enum cipher_method method, bool enc) {
    DCHECK_GT(method, CRYPTO_PLAINTEXT);
    if (enc) {
      encrypter = crypto::Encrypter::CreateFromCipherSuite(method);
    } else {
      decrypter = crypto::Decrypter::CreateFromCipherSuite(method);
    }
  }

  static int parse_key(const std::string &key, uint8_t *skey, size_t skey_len) {
    const char *base64 = key.c_str();
    const size_t base64_len = key.size();
    uint32_t out_len = modp_b64_decode_len(base64_len);
    std::unique_ptr<char[]> out = std::make_unique<char[]>(out_len);

    out_len = modp_b64_decode(out.get(), base64, out_len);
    if (out_len > 0 && out_len >= skey_len) {
      memcpy(skey, out.get(), skey_len);
      return skey_len;
    }
    return 0;
  }

  /// The master key can be input directly from user or generated from a password.
  /// The key derivation is still following EVP_BytesToKey(3) in OpenSSL.
  static int derive_key(const std::string &key, uint8_t *skey,
                        size_t skey_len) {
    const char *pass = key.c_str();
    size_t datal = key.size();
    MD5Context c;
    unsigned char md_buf[MD_MAX_SIZE_256];
    int addmd;
    unsigned int i, j, mds;

    if (key.empty()) {
      return skey_len;
    }

    mds = 16;
    MD5Init(&c);

    for (j = 0, addmd = 0; j < skey_len; addmd++) {
      MD5Init(&c);
      if (addmd) {
        MD5Update(&c, md_buf, mds);
      }
      MD5Update(&c, (uint8_t *)pass, datal);
      MD5Final((MD5Digest *)md_buf, &c);

      for (i = 0; i < mds; i++, j++) {
        if (j >= skey_len)
          break;
        skey[j] = md_buf[i];
      }
    }

    return skey_len;
  }

  /// EncryptPacket is a function that takes a secret key,
  /// a non-secret nonce, a message, and produces ciphertext and authentication tag.
  /// Nonce (NoncePrefix + packet_number, or vice versa) must be unique for a given key in each invocation.
  int EncryptPacket(uint64_t packet_number,
                    uint8_t *c, size_t *clen, const uint8_t *m, size_t mlen) {
    int err = 0;

    if (!encrypter->EncryptPacket(packet_number, nullptr, 0U, (const char *)m,
                                  mlen, (char *)c, clen, *clen)) {
      err = -1;
    }

    return err;
  }

  /// DecryptPacket is a function that takes a secret key,
  /// non-secret nonce, ciphertext, authentication tag, and produces original message.
  /// If any of the input is tampered with, decryption will fail.
  int DecryptPacket(uint64_t packet_number,
                    uint8_t *p, size_t *plen, const uint8_t *m, size_t mlen) {
    int err = 0;

    if (!decrypter->DecryptPacket(packet_number, nullptr, 0U, (const char *)m,
                                  mlen, (char *)p, plen, *plen)) {
      err = -1;
    }

    return err;
  }

  void SetKey(const uint8_t *key, size_t key_len) {
    if (encrypter) {
      encrypter->SetKey((const char *)key, key_len);
    }
    if (decrypter) {
      decrypter->SetKey((const char *)key, key_len);
    }
  }

  void SetNoncePrefix(const uint8_t *nonce_prefix, size_t nonce_prefix_len) {
    if (encrypter) {
      encrypter->SetNoncePrefix(reinterpret_cast<const char*>(nonce_prefix),
                                std::min(nonce_prefix_len, encrypter->GetNoncePrefixSize()));
    }
    if (decrypter) {
      decrypter->SetNoncePrefix(reinterpret_cast<const char*>(nonce_prefix),
                                std::min(nonce_prefix_len, decrypter->GetNoncePrefixSize()));
    }
  }

  void SetIV(const uint8_t *iv, size_t iv_len) {
    if (encrypter) {
      encrypter->SetIV(reinterpret_cast<const char*>(iv), iv_len);
    }
    if (decrypter) {
      decrypter->SetIV(reinterpret_cast<const char*>(iv), iv_len);
    }
  }

  size_t GetKeySize() const {
    return encrypter ? encrypter->GetKeySize() : decrypter->GetKeySize();
  }

  size_t GetNoncePrefixSize() const {
    return encrypter ? encrypter->GetNoncePrefixSize() : decrypter->GetNoncePrefixSize();
  }

  size_t GetIVSize() const {
    return encrypter ? encrypter->GetIVSize() : decrypter->GetIVSize();
  }

  size_t GetTagSize() const {
    return encrypter ? encrypter->GetTagSize() : decrypter->GetTagSize();
  }

  const uint8_t* GetKey() const {
    return encrypter ? encrypter->GetKey() : decrypter->GetKey();
  }

  const uint8_t* GetNoncePrefix() const {
    return encrypter ? encrypter->GetNoncePrefix() : decrypter->GetNoncePrefix();
  }

  std::unique_ptr<crypto::Encrypter> encrypter;
  std::unique_ptr<crypto::Decrypter> decrypter;
};

cipher::cipher(const std::string &key, const std::string &password,
               enum cipher_method method, bool enc)
    : salt_(), key_(), counter_(), init_(false) {
  impl_ = new cipher_impl(method, enc);
  key_bitlen_ = impl_->GetKeySize() * 8;
  key_len_ = !key.empty()
                 ? cipher_impl::parse_key(key, key_, key_bitlen_ / 8)
                 : cipher_impl::derive_key(password, key_, key_bitlen_ / 8);

#ifndef NDEBUG
  DumpHex("KEY", key_, key_len_);
#endif

  tag_len_ = impl_->GetTagSize();
}

cipher::~cipher() { delete impl_; }

void cipher::decrypt(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
                     size_t capacity) {
  if (!chunk_) {
    chunk_ = IOBuf::create(capacity);
  }
  chunk_->reserve(0, ciphertext->length());
  memcpy(chunk_->mutable_tail(), ciphertext->data(), ciphertext->length());
  chunk_->append(ciphertext->length());

  if (!init_) {
    if (chunk_->length() < key_len_) {
      return;
    }
    decrypt_salt(chunk_.get());

    init_ = true;
  }

  plaintext = IOBuf::create(capacity);
  while (!chunk_->empty()) {
    uint64_t counter = counter_;
    size_t chunk_len = 0;

    counter = counter_;

    if (!chunk_decrypt_frame(&counter,
                             plaintext.get(), chunk_.get(), &chunk_len)) {
      break;
    }

    counter_ = counter;
    chunk_->trimStart(chunk_len);
  }
  chunk_->retreat(chunk_->headroom());
}

void cipher::encrypt(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
                     size_t capacity) {
  ciphertext = IOBuf::create(capacity);

  if (!init_) {
    encrypt_salt(ciphertext.get());
    init_ = true;
  }

  size_t clen = 2 * tag_len_ + CHUNK_SIZE_LEN + plaintext->length();

  ciphertext->reserve(0, clen);

  uint64_t counter = counter_;

  counter = counter_;

  // TBD better to apply MTU-like things
  chunk_encrypt_frame(&counter, plaintext, ciphertext.get());

  counter_ = counter;
}

void cipher::decrypt_salt(IOBuf *chunk) {
  DCHECK(!init_);
  size_t salt_len = key_len_;
  VLOG(4) << "decrypt: salt: " << salt_len;

  memcpy(salt_, chunk->data(), salt_len);
  chunk->trimStart(salt_len);
  chunk->retreat(salt_len);
  set_key_aead(salt_, salt_len);

#ifndef NDEBUG
  DumpHex("DE-SALT", salt_, salt_len);
#endif
}

void cipher::encrypt_salt(IOBuf *chunk) {
  DCHECK(!init_);

  size_t salt_len = key_len_;
  VLOG(4) << "encrypt: salt: " << salt_len;
  RandBytes(salt_, key_len_);
  chunk->reserve(salt_len, 0);
  chunk->prepend(salt_len);
  memcpy(chunk->mutable_data(), salt_, salt_len);
  set_key_aead(salt_, salt_len);

#ifndef NDEBUG
  DumpHex("EN-SALT", salt_, salt_len);
#endif
}

bool cipher::chunk_decrypt_frame(uint64_t* counter,
                                 IOBuf *plaintext, const IOBuf *ciphertext,
                                 size_t *consumed_len) const {
  int err;
  size_t mlen;
  size_t tlen = tag_len_;
  size_t plen = 0;

  VLOG(4) << "decrypt: current chunk: " << ciphertext->length()
          << " expected: " << (2 * tlen + CHUNK_SIZE_LEN);


  if (ciphertext->length() <= 2 * tlen + CHUNK_SIZE_LEN) {
    return false;
  }

  uint8_t len_buf[2];
  err = impl_->DecryptPacket(*counter, len_buf, &plen, ciphertext->data(),
                             CHUNK_SIZE_LEN + tlen);
  if (err) {
    return false;
  }
  DCHECK_EQ(plen, CHUNK_SIZE_LEN);

  mlen = ntohs(*(uint16_t *)len_buf);
  mlen = mlen & CHUNK_SIZE_MASK;
  plaintext->reserve(0, mlen);

  if (mlen == 0) {
    return false;
  }

  size_t chunk_len = 2 * tlen + CHUNK_SIZE_LEN + mlen;

  VLOG(4) << "decrypt: current chunk: " << ciphertext->length()
          << " expected: " << chunk_len;

  if (ciphertext->length() < chunk_len) {
    return false;
  }

  (*counter)++;

  err = impl_->DecryptPacket(*counter, plaintext->mutable_tail(), &plen,
                             ciphertext->data() + CHUNK_SIZE_LEN + tlen,
                             mlen + tlen);
  if (err) {
    return false;
  }
  DCHECK_EQ(plen, mlen);

  (*counter)++;

  *consumed_len = chunk_len;

  plaintext->append(plen);

  return true;
}

bool cipher::chunk_encrypt_frame(uint64_t* counter,
                                 const IOBuf *plaintext, IOBuf *ciphertext) const {
  size_t tlen = tag_len_;

  DCHECK_LE(plaintext->length(), CHUNK_SIZE_MASK);

  int err;
  size_t clen;
  uint8_t len_buf[CHUNK_SIZE_LEN] = {};
  uint16_t t = htons(plaintext->length() & CHUNK_SIZE_MASK);
  memcpy(len_buf, &t, CHUNK_SIZE_LEN);

  clen = CHUNK_SIZE_LEN + tlen;
  VLOG(4) << "encrypt: current chunk: " << clen
          << " original: " << sizeof(len_buf);
  err = impl_->EncryptPacket(*counter, ciphertext->mutable_tail(), &clen, len_buf,
                             CHUNK_SIZE_LEN);
  if (err) {
    return false;
  }

  DCHECK_EQ(clen, CHUNK_SIZE_LEN + tlen);
  ciphertext->append(clen);

  (*counter)++;

  clen = plaintext->length() + tlen;
  VLOG(4) << "encrypt: current chunk: " << clen
          << " original: " << plaintext->length();
  err = impl_->EncryptPacket(*counter, ciphertext->mutable_tail(), &clen, plaintext->data(),
                             plaintext->length());
  if (err) {
    return false;
  }

  DCHECK_EQ(clen, plaintext->length() + tlen);

  (*counter)++;

  ciphertext->append(clen);

  return true;
}

void cipher::set_key_aead(const uint8_t *salt, size_t salt_len) {
  DCHECK_EQ(salt_len, key_len_);
  uint8_t skey[MAX_KEY_LENGTH];
  int err = crypto_hkdf(salt, salt_len, key_, key_len_, (uint8_t *)SUBKEY_INFO,
                        sizeof(SUBKEY_INFO) - 1, skey, key_len_);
  if (err) {
    LOG(FATAL) << "Unable to generate subkey";
  }

  counter_ = 0;
  uint8_t nonce[MAX_NONCE_LENGTH];
  memset(nonce, 0, impl_->GetIVSize());

  impl_->SetKey(skey, key_len_);
  impl_->SetIV(nonce, impl_->GetIVSize());

#ifndef NDEBUG
  DumpHex("SKEY", impl_->GetKey(), impl_->GetKeySize());
  DumpHex("NONCE_PREFIX", impl_->GetNoncePrefix(), impl_->GetNoncePrefixSize());
#endif
}

enum cipher_method cipher_method_in_use;
