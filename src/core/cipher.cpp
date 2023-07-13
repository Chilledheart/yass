// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/cipher.hpp"

#include <memory>

#include <openssl/base64.h>
#include <openssl/md5.h>

#include "core/hkdf_sha1.hpp"
#include "core/logging.hpp"
#include "core/rand_util.hpp"
#include "core/sys_byteorder.h"
#include "crypto/decrypter.hpp"
#include "crypto/encrypter.hpp"

#define CHUNK_SIZE_LEN 2U
#define CHUNK_SIZE_MASK 0x3FFFU

class cipher_impl {
 public:
  cipher_impl(enum cipher_method method, bool enc) {
    DCHECK_GT(method, CRYPTO_INVALID);
    if (enc) {
      encrypter = crypto::Encrypter::CreateFromCipherSuite(method);
    } else {
      decrypter = crypto::Decrypter::CreateFromCipherSuite(method);
    }
  }

  static int parse_key(const std::string& key, uint8_t* skey, size_t skey_len) {
    const char* base64 = key.c_str();
    const size_t base64_len = key.size();
    size_t out_len;

    if (!EVP_DecodedLength(&out_len, base64_len)) {
      LOG(WARNING) << "Invalid base64 len: " << base64_len;
      return 0;
    }
    std::unique_ptr<uint8_t[]> out = std::make_unique<uint8_t[]>(out_len);

    out_len =
        EVP_DecodeBase64(out.get(), &out_len, out_len,
                         reinterpret_cast<const uint8_t*>(base64), base64_len);
    if (out_len > 0 && out_len >= skey_len) {
      memcpy(skey, out.get(), skey_len);
      return skey_len;
    }
    return 0;
  }

  /// The master key can be input directly from user or generated from a
  /// password. The key derivation is still following EVP_BytesToKey(3) in
  /// OpenSSL.
  static int derive_key(const std::string& key,
                        uint8_t* skey,
                        size_t skey_len) {
    const char* pass = key.c_str();
    size_t datal = key.size();
    MD5_CTX c;
    uint8_t md_buf[MD5_DIGEST_LENGTH];
    int addmd;
    unsigned int i, j, mds;

    if (key.empty()) {
      return skey_len;
    }

    mds = 16;
    MD5_Init(&c);

    for (j = 0, addmd = 0; j < skey_len; addmd++) {
      MD5_Init(&c);
      if (addmd) {
        MD5_Update(&c, md_buf, mds);
      }
      MD5_Update(&c, reinterpret_cast<const uint8_t*>(pass), datal);
      MD5_Final(md_buf, &c);

      for (i = 0; i < mds; i++, j++) {
        if (j >= skey_len)
          break;
        skey[j] = md_buf[i];
      }
    }

    return skey_len;
  }

  /// EncryptPacket is a function that takes a secret key,
  /// a non-secret nonce, a message, and produces ciphertext and authentication
  /// tag. Nonce (NoncePrefix + packet_number, or vice versa) must be unique for
  /// a given key in each invocation.
  int EncryptPacket(uint64_t packet_number,
                    uint8_t* c,
                    size_t* clen,
                    const uint8_t* m,
                    size_t mlen) {
    int err = 0;

    if (!encrypter->EncryptPacket(packet_number, nullptr, 0U,
                                  reinterpret_cast<const char*>(m), mlen,
                                  reinterpret_cast<char*>(c), clen, *clen)) {
      err = -1;
    }

    return err;
  }

  /// DecryptPacket is a function that takes a secret key,
  /// non-secret nonce, ciphertext, authentication tag, and produces original
  /// message. If any of the input is tampered with, decryption will fail.
  int DecryptPacket(uint64_t packet_number,
                    uint8_t* p,
                    size_t* plen,
                    const uint8_t* m,
                    size_t mlen) {
    int err = 0;

    if (!decrypter->DecryptPacket(packet_number, nullptr, 0U,
                                  reinterpret_cast<const char*>(m), mlen,
                                  reinterpret_cast<char*>(p), plen, *plen)) {
      err = -1;
    }

    return err;
  }

  bool SetKey(const uint8_t* key, size_t key_len) {
    if (encrypter) {
      return encrypter->SetKey(reinterpret_cast<const char*>(key), key_len);
    }
    if (decrypter) {
      return decrypter->SetKey(reinterpret_cast<const char*>(key), key_len);
    }
    return false;
  }

  bool SetNoncePrefix(const uint8_t* nonce_prefix, size_t nonce_prefix_len) {
    if (encrypter) {
      return encrypter->SetNoncePrefix(
          reinterpret_cast<const char*>(nonce_prefix),
          std::min(nonce_prefix_len, encrypter->GetNoncePrefixSize()));
    }
    if (decrypter) {
      return decrypter->SetNoncePrefix(
          reinterpret_cast<const char*>(nonce_prefix),
          std::min(nonce_prefix_len, decrypter->GetNoncePrefixSize()));
    }
    return false;
  }

  bool SetIV(const uint8_t* iv, size_t iv_len) {
    if (encrypter) {
      return encrypter->SetIV(reinterpret_cast<const char*>(iv), iv_len);
    }
    if (decrypter) {
      return decrypter->SetIV(reinterpret_cast<const char*>(iv), iv_len);
    }
    return false;
  }

  size_t GetKeySize() const {
    return encrypter ? encrypter->GetKeySize() : decrypter->GetKeySize();
  }

  size_t GetNoncePrefixSize() const {
    return encrypter ? encrypter->GetNoncePrefixSize()
                     : decrypter->GetNoncePrefixSize();
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
    return encrypter ? encrypter->GetNoncePrefix()
                     : decrypter->GetNoncePrefix();
  }

  std::unique_ptr<crypto::Encrypter> encrypter;
  std::unique_ptr<crypto::Decrypter> decrypter;
};

cipher::cipher(const std::string& key,
               const std::string& password,
               enum cipher_method method,
               cipher_visitor_interface *visitor,
               std::vector<std::shared_ptr<IOBuf>> *mempool,
               bool enc)
    : salt_(), key_(), counter_(), init_(false), visitor_(visitor), mempool_(mempool) {
  DCHECK(is_valid_cipher_method(method));
  VLOG(3) << "cipher: " << (enc ? "encoder" : "decoder")
          << " create with key \"" << key << "\" password \"" << password
          << "\" cipher_method: " << to_cipher_method_str(method);

  impl_ = std::make_unique<cipher_impl>(method, enc);
  key_bitlen_ = impl_->GetKeySize() * 8;
  key_len_ = !key.empty()
                 ? cipher_impl::parse_key(key, key_, key_bitlen_ / 8)
                 : cipher_impl::derive_key(password, key_, key_bitlen_ / 8);

#ifndef NDEBUG
  DumpHex("cipher: KEY", key_, key_len_);
#endif

  tag_len_ = impl_->GetTagSize();
}

cipher::~cipher() = default;

void cipher::process_bytes(std::shared_ptr<IOBuf> ciphertext) {
  if (!chunk_) {
    chunk_ = IOBuf::create(SOCKET_DEBUF_SIZE);
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

  while (!chunk_->empty()) {
    std::shared_ptr<IOBuf> plaintext;
    if (mempool_  && !mempool_->empty()) {
      plaintext = mempool_->back();
      mempool_->pop_back();
      plaintext->clear();
      plaintext->reserve(0, SOCKET_BUF_SIZE);
    } else {
      plaintext = IOBuf::create(SOCKET_BUF_SIZE);
    }

    uint64_t counter = counter_;

    int ret = chunk_decrypt_frame(&counter, plaintext.get(), chunk_.get());

    if (ret == -EAGAIN) {
      break;
    }

    if (ret < 0) {
      visitor_->on_protocol_error();
      break;
    }

    counter_ = counter;

    // DISCARD
    if (!visitor_->on_received_data(plaintext)) {
      break;
    }
  }
  chunk_->retreat(chunk_->headroom());
}

void cipher::encrypt(const uint8_t* plaintext_data,
                     size_t plaintext_size,
                     std::shared_ptr<IOBuf> ciphertext) {
  DCHECK(ciphertext);

  if (!init_) {
    encrypt_salt(ciphertext.get());
    init_ = true;
  }

  size_t clen = 2 * tag_len_ + CHUNK_SIZE_LEN + plaintext_size;

  ciphertext->reserve(0, clen);

  uint64_t counter = counter_;

  counter = counter_;

  // TBD better to apply MTU-like things
  int ret = chunk_encrypt_frame(&counter, plaintext_data,
                                plaintext_size, ciphertext.get());
  if (ret < 0) {
    visitor_->on_protocol_error();
    return;
  }

  counter_ = counter;
}

void cipher::decrypt_salt(IOBuf* chunk) {
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

void cipher::encrypt_salt(IOBuf* chunk) {
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

int cipher::chunk_decrypt_frame(uint64_t* counter,
                                IOBuf* plaintext,
                                IOBuf* ciphertext) const {
  int err;
  size_t mlen;
  size_t tlen = tag_len_;
  size_t plen = 0;
  size_t clen = CHUNK_SIZE_LEN + tlen;

  VLOG(4) << "decrypt: 1st chunk: origin: " << CHUNK_SIZE_LEN
          << " encrypted: " << clen
          << " actual: " << ciphertext->length();

  if (ciphertext->length() < tlen + CHUNK_SIZE_LEN + tlen) {
    return -EAGAIN;
  }

  union {
    uint8_t buf[2];
    uint16_t cover;
  } len;

  static_assert(sizeof(len) == CHUNK_SIZE_LEN, "Chunk Size not matched");

  plen = sizeof(len.cover);

  err = impl_->DecryptPacket(*counter, len.buf, &plen, ciphertext->data(),
                             clen);

  if (err) {
    return -EBADMSG;
  }

  DCHECK_EQ(plen, CHUNK_SIZE_LEN);

  mlen = ntohs(len.cover);
  mlen = mlen & CHUNK_SIZE_MASK;

  if (mlen == 0) {
    return -EBADMSG;
  }

  ciphertext->trimStart(clen);
  plaintext->reserve(0, mlen);

  clen = tlen + mlen;

  VLOG(4) << "decrypt: 2nd chunk: origin: " << mlen
          << " encrypted: " << clen
          << " actual: " << ciphertext->length();

  if (ciphertext->length() < clen) {
    ciphertext->prepend(CHUNK_SIZE_LEN + tlen);
    return -EAGAIN;
  }

  (*counter)++;

  plen = plaintext->capacity();
  err = impl_->DecryptPacket(*counter, plaintext->mutable_tail(), &plen,
                             ciphertext->data(), clen);
  if (err) {
    ciphertext->prepend(CHUNK_SIZE_LEN + tlen);
    return -EBADMSG;
  }

  DCHECK_EQ(plen, mlen);

  (*counter)++;

  ciphertext->trimStart(clen);

  plaintext->append(plen);

  return 0;
}

int cipher::chunk_encrypt_frame(uint64_t* counter,
                                const uint8_t* plaintext_data,
                                size_t plaintext_size,
                                IOBuf* ciphertext) const {
  size_t tlen = tag_len_;

  DCHECK_LE(plaintext_size, CHUNK_SIZE_MASK);

  int err;
  size_t clen = CHUNK_SIZE_LEN + tlen;

  union {
    uint8_t buf[2];
    uint16_t cover;
  } len;

  static_assert(sizeof(len) == CHUNK_SIZE_LEN, "Chunk Size not matched");

  len.cover = htons(plaintext_size & CHUNK_SIZE_MASK);

  VLOG(4) << "encrypt: 1st chunk: origin: " << CHUNK_SIZE_LEN
          << " encrypted: " << clen;

  ciphertext->reserve(0, clen);

  err = impl_->EncryptPacket(*counter, ciphertext->mutable_tail(), &clen,
                             len.buf, CHUNK_SIZE_LEN);
  if (err) {
    return -EBADMSG;
  }

  DCHECK_EQ(clen, CHUNK_SIZE_LEN + tlen);

  ciphertext->append(clen);

  (*counter)++;

  clen = plaintext_size + tlen;

  VLOG(4) << "encrypt: 2nd chunk: origin: " << plaintext_size
          << " encrypted: " << clen;

  ciphertext->reserve(0, clen);
  // FIXME it is a bug with crypto layer
  memset(ciphertext->mutable_tail(), 0, clen);

  err = impl_->EncryptPacket(*counter, ciphertext->mutable_tail(), &clen,
                             plaintext_data, plaintext_size);
  if (err) {
    ciphertext->trimEnd(CHUNK_SIZE_LEN + tlen);
    return -EBADMSG;
  }

  DCHECK_EQ(clen, plaintext_size + tlen);

  ciphertext->append(clen);

  (*counter)++;

  return 0;
}

void cipher::set_key_aead(const uint8_t* salt, size_t salt_len) {
  DCHECK_EQ(salt_len, key_len_);
  uint8_t skey[MAX_KEY_LENGTH] = {};
  int err = crypto_hkdf(salt, salt_len, key_, key_len_,
                        reinterpret_cast<const uint8_t*>(SUBKEY_INFO),
                        sizeof(SUBKEY_INFO) - 1, skey, key_len_);
  if (err) {
    LOG(FATAL) << "Unable to generate subkey";
  }

  counter_ = 0;
  uint8_t nonce[MAX_NONCE_LENGTH] = {};

  if (!impl_->SetKey(skey, key_len_)) {
    LOG(WARNING) << "SetKey Failed";
  }

  if (!impl_->SetNoncePrefix(nonce, impl_->GetNoncePrefixSize())) {
    LOG(WARNING) << "SetNoncePrefix Failed";
  }

#ifndef NDEBUG
  DumpHex("SKEY", impl_->GetKey(), impl_->GetKeySize());
  DumpHex("NONCE_PREFIX", impl_->GetNoncePrefix(), impl_->GetNoncePrefixSize());
#endif
}
