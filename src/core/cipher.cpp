//
// cipher.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "cipher.hpp"
#include "protocol.hpp"

extern "C" {
#include "base64.h"
}
#include <vector>

#include <sodium/core.h>
#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>
#include <sodium/crypto_stream_chacha20.h>
#include <sodium/crypto_stream_salsa20.h>
#include <sodium/randombytes.h>

#include <openssl/md5.h>
#include <openssl/sha.h>
#define internal_md5_ctx MD5_CTX
#define internal_md5_init MD5_Init
#define internal_md5_update MD5_Update
#define internal_md5_final MD5_Final

#define internal_sha1_ctx SHA_CTX
#define internal_sha1_init SHA1_Init
#define internal_sha1_update SHA1_Update
#define internal_sha1_final SHA1_Final

#define SODIUM_BLOCK_SIZE 64
#define SUBKEY_INFO "ss-subkey"

#define CHUNK_SIZE_LEN 2U
#define CHUNK_SIZE_MASK 0x3FFFU

#define MAX_MD_SIZE 64     /* longest known is SHA512 */
#define MD_MAX_SIZE_256 32 /* longest known is SHA256 or less */

#define HASH_BLOCK_SIZE 128
#define HASH_BLOCK_SIZE_256 64

#define OUTPUT_SIZE_SHA1 20

#ifdef __GNUC__
#define UNUSED_F __attribute__((unused))
#endif

#ifdef _MSC_VER
#define UNUSED_F [[maybe_unused]]
#endif

cipher::cipher_method cipher_method;

static constexpr int nonce_size[cipher::MAX_CIPHER_METHOD] = {0,  8,  8,
                                                              12, 12, 24};
static constexpr int key_size[cipher::MAX_CIPHER_METHOD] = {0,  32, 32,
                                                            32, 32, 32};
static constexpr int tag_size[cipher::MAX_CIPHER_METHOD] = {0, 0, 0, 0, 16, 16};

static int parse_key(const std::string &key, uint8_t *skey, size_t skey_len) {
  const char *base64 = key.c_str();
  const size_t base64_len = key.size();
  uint32_t out_len = BASE64_SIZE(base64_len);
  std::vector<uint8_t> out;
  out.resize(out_len);

  out_len = base64_decode(out.data(), base64, out_len);
  if (out_len > 0 && out_len >= skey_len) {
    memcpy(skey, out.data(), skey_len);
    return skey_len;
  }
  return 0;
}

static int derive_key(const std::string &key, uint8_t *skey, size_t skey_len) {
  const char *pass = key.c_str();
  size_t datal = key.size();
  internal_md5_ctx c;
  unsigned char md_buf[MD_MAX_SIZE_256];
  int addmd;
  unsigned int i, j, mds;

  if (key.empty())
    return skey_len;

  mds = 16;
  internal_md5_init(&c);

  for (j = 0, addmd = 0; j < skey_len; addmd++) {
    internal_md5_init(&c);
    if (addmd) {
      internal_md5_update(&c, md_buf, mds);
    }
    internal_md5_update(&c, (uint8_t *)pass, datal);
    internal_md5_final(&(md_buf[0]), &c);

    for (i = 0; i < mds; i++, j++) {
      if (j >= skey_len)
        break;
      skey[j] = md_buf[i];
    }
  }

  return skey_len;
}

static int hmac_sha1_starts(internal_sha1_ctx *ctx, unsigned char *ipad,
                            unsigned char *opad, const unsigned char *key,
                            size_t keylen) {
  int ret = 0;
  unsigned char sum[MD_MAX_SIZE_256];
  size_t i;

  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  if (keylen > (size_t)HASH_BLOCK_SIZE_256) {
    internal_sha1_init(ctx);
    internal_sha1_update(ctx, key, keylen);
    internal_sha1_final(sum, ctx);

    keylen = OUTPUT_SIZE_SHA1;
    key = sum;
  }

  memset(ipad, 0x36, HASH_BLOCK_SIZE_256);
  memset(opad, 0x5C, HASH_BLOCK_SIZE_256);

  for (i = 0; i < keylen; i++) {
    ipad[i] = (unsigned char)(ipad[i] ^ key[i]);
    opad[i] = (unsigned char)(opad[i] ^ key[i]);
  }

  internal_sha1_init(ctx);
  internal_sha1_update(ctx, ipad, HASH_BLOCK_SIZE_256);

  memset(sum, 0, sizeof(sum));

  return ret;
}

static int hmac_sha1_update(internal_sha1_ctx *ctx, unsigned char *ipad,
                            unsigned char *opad, const unsigned char *input,
                            size_t ilen) {
  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  internal_sha1_update(ctx, input, ilen);

  return 0;
}

static int hmac_sha1_finish(internal_sha1_ctx *ctx, unsigned char *ipad,
                            unsigned char *opad, unsigned char *output) {
  unsigned char tmp[MD_MAX_SIZE_256];

  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  internal_sha1_final(tmp, ctx);

  internal_sha1_init(ctx);
  internal_sha1_update(ctx, opad, HASH_BLOCK_SIZE_256);
  internal_sha1_update(ctx, tmp, OUTPUT_SIZE_SHA1);
  internal_sha1_final(output, ctx);

  return 0;
}

static UNUSED_F
int hmac_sha1_reset(internal_sha1_ctx *ctx, unsigned char *ipad,
                    unsigned char *opad) {
  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  internal_sha1_init(ctx);
  internal_sha1_update(ctx, ipad, HASH_BLOCK_SIZE_256);

  return 0;
}

static int hmac_sha1(const unsigned char *key, size_t keylen,
                     const unsigned char *input, size_t ilen,
                     unsigned char *output) {
  internal_sha1_ctx ctx;
  unsigned char ipad[HASH_BLOCK_SIZE_256], opad[HASH_BLOCK_SIZE_256];
  int ret;

  internal_sha1_init(&ctx);

  if ((ret = hmac_sha1_starts(&ctx, ipad, opad, key, keylen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_update(&ctx, ipad, opad, input, ilen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_finish(&ctx, ipad, opad, output)) != 0)
    goto cleanup;

cleanup:

  return ret;
}

/*
 * Spec: http://shadowsocks.org/en/spec/AEAD-Ciphers.html
 *
 * The way Shadowsocks using AEAD ciphers is specified in SIP004 and amended in
 * SIP007. SIP004 was proposed by @Mygod with design inspirations from
 * @wongsyrone, @Noisyfox and @breakwa11. SIP007 was proposed by @riobard with
 * input from
 * @madeye, @Mygod, @wongsyrone, and many others.
 *
 * Key Derivation
 *
 * HKDF_SHA1 is a function that takes a secret key, a non-secret salt, an info
 * string, and produces a subkey that is cryptographically strong even if the
 * input secret key is weak.
 *
 *      HKDF_SHA1(key, salt, info) => subkey
 *
 * The info string binds the generated subkey to a specific application context.
 * In our case, it must be the string "ss-subkey" without quotes.
 *
 * We derive a per-session subkey from a pre-shared master key using HKDF_SHA1.
 * Salt must be unique through the entire life of the pre-shared master key.
 *
 * TCP
 *
 * An AEAD encrypted TCP stream starts with a randomly generated salt to derive
 * the per-session subkey, followed by any number of encrypted chunks. Each
 * chunk has the following structure:
 *
 *      [encrypted payload length][length tag][encrypted payload][payload tag]
 *
 * Payload length is a 2-byte big-endian unsigned integer capped at 0x3FFF. The
 * higher two bits are reserved and must be set to zero. Payload is therefore
 * limited to 16*1024 - 1 bytes.
 *
 * The first AEAD encrypt/decrypt operation uses a counting nonce starting from
 * 0. After each encrypt/decrypt operation, the nonce is incremented by one as
 * if it were an unsigned little-endian integer. Note that each TCP chunk
 * involves two AEAD encrypt/decrypt operation: one for the payload length, and
 * one for the payload. Therefore each chunk increases the nonce twice.
 *
 * UDP
 *
 * An AEAD encrypted UDP packet has the following structure:
 *
 *      [salt][encrypted payload][tag]
 *
 * The salt is used to derive the per-session subkey and must be generated
 * randomly to ensure uniqueness. Each UDP packet is encrypted/decrypted
 * independently, using the derived subkey and a nonce with all zero bytes.
 *
 */

static int crypto_hkdf_extract(const unsigned char *salt, int salt_len,
                               const unsigned char *ikm, int ikm_len,
                               unsigned char *prk);

static int crypto_hkdf_expand(const unsigned char *prk, int prk_len,
                              const unsigned char *info, int info_len,
                              unsigned char *okm, int okm_len);

/* HKDF-Extract + HKDF-Expand */
static int crypto_hkdf(const unsigned char *salt, int salt_len,
                       const unsigned char *ikm, int ikm_len,
                       const unsigned char *info, int info_len,
                       unsigned char *okm, int okm_len) {
  unsigned char prk[MD_MAX_SIZE_256];

  return crypto_hkdf_extract(salt, salt_len, ikm, ikm_len, prk) ||
         crypto_hkdf_expand(prk, OUTPUT_SIZE_SHA1, info, info_len, okm,
                            okm_len);
}

/* HKDF-Extract(salt, IKM) -> PRK */
int crypto_hkdf_extract(const unsigned char *salt, int salt_len,
                        const unsigned char *ikm, int ikm_len,
                        unsigned char *prk) {
  int hash_len;
  unsigned char null_salt[MD_MAX_SIZE_256] = {'\0'};

  if (salt_len < 0) {
    return -1;
  }

  hash_len = OUTPUT_SIZE_SHA1;

  if (salt == NULL) {
    salt = null_salt;
    salt_len = hash_len;
  }

  return hmac_sha1(salt, salt_len, ikm, ikm_len, prk);
}

/* HKDF-Expand(PRK, info, L) -> OKM */
int crypto_hkdf_expand(const unsigned char *prk, int prk_len,
                       const unsigned char *info, int info_len,
                       unsigned char *okm, int okm_len) {
  int hash_len;
  int N;
  int T_len = 0, where = 0, i, ret;
  internal_sha1_ctx ctx;
  unsigned char ipad[HASH_BLOCK_SIZE_256], opad[HASH_BLOCK_SIZE_256];
  unsigned char T[MD_MAX_SIZE_256];

  if (info_len < 0 || okm_len < 0 || okm == NULL) {
    return -1;
  }

  hash_len = OUTPUT_SIZE_SHA1;

  if (prk_len < hash_len) {
    return -1;
  }

  if (info == NULL) {
    info = (const unsigned char *)"";
  }

  N = okm_len / hash_len;

  if ((okm_len % hash_len) != 0) {
    N++;
  }

  if (N > 255) {
    return -1;
  }

  internal_sha1_init(&ctx);

  /* Section 2.3. */
  for (i = 1; i <= N; i++) {
    unsigned char c = i;

    ret = hmac_sha1_starts(&ctx, ipad, opad, prk, prk_len) ||
          hmac_sha1_update(&ctx, ipad, opad, T, T_len) ||
          hmac_sha1_update(&ctx, ipad, opad, info, info_len) ||
          /* The constant concatenated to the end of each T(n) is a single
           * octet. */
          hmac_sha1_update(&ctx, ipad, opad, &c, 1) ||
          hmac_sha1_finish(&ctx, ipad, opad, T);

    if (ret != 0) {
      goto cleanup;
    }

    memcpy(okm + where, T, (i != N) ? hash_len : (okm_len - where));
    where += hash_len;
    T_len = hash_len;
  }

cleanup:

  return 0;
}

static int crypto_stream_xor_ic(uint8_t *c, const uint8_t *m, uint64_t mlen,
                                const uint8_t *n, uint64_t ic, const uint8_t *k,
                                int method) {
  switch (method) {
  case cipher::SALSA20:
    return ::crypto_stream_salsa20_xor_ic(c, m, mlen, n, ic, k);
  case cipher::CHACHA20:
    return ::crypto_stream_chacha20_xor_ic(c, m, mlen, n, ic, k);
  case cipher::CHACHA20IETF:
    return ::crypto_stream_chacha20_ietf_xor_ic(c, m, mlen, n, (uint32_t)ic, k);
  }
  // always return 0
  return 0;
}

static int aead_cipher_encrypt(uint8_t *c, size_t *clen, const uint8_t *m,
                               size_t mlen, uint8_t *ad, size_t adlen,
                               const uint8_t *n, const uint8_t *k, int method) {
  int err = 0;
  unsigned long long long_clen = 0;

  switch (method) {
  case cipher::CHACHA20POLY1305IETF:
    err = ::crypto_aead_chacha20poly1305_ietf_encrypt(c, &long_clen, m, mlen,
                                                      ad, adlen, NULL, n, k);
    *clen = (size_t)long_clen;
    break;
  case cipher::XCHACHA20POLY1305IETF:
    err = ::crypto_aead_xchacha20poly1305_ietf_encrypt(c, &long_clen, m, mlen,
                                                       ad, adlen, NULL, n, k);
    *clen = (size_t)long_clen;
    break;
  default:
    return -1;
  }

  return err;
}

static int aead_cipher_decrypt(uint8_t *p, size_t *plen, const uint8_t *m,
                               size_t mlen, uint8_t *ad, size_t adlen,
                               const uint8_t *n, const uint8_t *k, int method) {
  int err = -1;
  unsigned long long long_plen = 0;

  switch (method) {
  case cipher::CHACHA20POLY1305IETF:
    err = ::crypto_aead_chacha20poly1305_ietf_decrypt(p, &long_plen, NULL, m,
                                                      mlen, ad, adlen, n, k);
    *plen = (size_t)long_plen; // it's safe to cast 64bit to 32bit length here
    break;
  case cipher::XCHACHA20POLY1305IETF:
    err = ::crypto_aead_xchacha20poly1305_ietf_decrypt(p, &long_plen, NULL, m,
                                                       mlen, ad, adlen, n, k);
    *plen = (size_t)long_plen; // it's safe to cast 64bit to 32bit length here
    break;
  default:
    return -1;
  }

  return err;
}

cipher::cipher(const std::string &key, const std::string &password,
               cipher_method method, bool enc)
    : key_(), method_(method), key_bitlen_(key_size[method] * 8),
      key_len_(!key.empty() ? parse_key(key, key_, key_bitlen_ / 8)
                            : derive_key(password, key_, key_bitlen_ / 8)),
      nonce_len_(nonce_size[method]), tag_len_(tag_size[method]), init_(false),
      nonce_left_(nonce_len_), skey_(), nonce_(), salt_(), counter_() {

  // Initialize sodium for random generator
  if (sodium_init() == -1) {
  }

  if (enc) {
    ::randombytes_buf(nonce_, nonce_len_);
    ::randombytes_buf(salt_, key_len_);
  }

#ifndef NDEBUG
  DumpHex("KEY", key_, key_len_);
#endif
}

void cipher::decrypt_stream(IOBuf *ciphertext,
                            std::unique_ptr<IOBuf> &plaintext,
                            size_t capacity = SOCKET_BUF_SIZE) {
  if (!init_) {
    if (ciphertext->length() + nonce_left_ > nonce_len_) {
      memcpy(nonce_ + nonce_len_ - nonce_left_, ciphertext->data(),
             nonce_left_);
      ciphertext->trimStart(nonce_len_);
      ciphertext->retreat(nonce_len_);

      /* mark as init done */
      set_key_stream();
      nonce_left_ = 0;
      init_ = true;

    } else {
      memcpy(nonce_ + nonce_len_ - nonce_left_, ciphertext->data(),
             ciphertext->length());
      ciphertext->trimStart(ciphertext->length());
      ciphertext->retreat(ciphertext->length());
      return;
    }
  }

  int padding = counter_ % SODIUM_BLOCK_SIZE;

  plaintext = IOBuf::create(capacity);
  plaintext->reserve(padding, padding);
  plaintext->append(ciphertext->length());
  // brealloc(plaintext, (plaintext->len + padding) * 2, capacity);

  if (padding) {
    // adding padding
    ciphertext->reserve(padding, 0);
    ciphertext->prepend(padding);

    sodium_memzero(ciphertext->mutable_data(), padding);
  }

  crypto_stream_xor_ic(plaintext->mutable_data(), ciphertext->data(),
                       ciphertext->length(), (const uint8_t *)nonce_,
                       counter_ / SODIUM_BLOCK_SIZE, skey_, method_);
  counter_ += ciphertext->length();

  if (padding) {
    // remove padding
    memmove(plaintext->mutable_data(), plaintext->data() + padding,
            plaintext->length());
  }
}

void cipher::encrypt_stream(IOBuf *plaintext,
                            std::unique_ptr<IOBuf> &ciphertext,
                            size_t capacity = SOCKET_BUF_SIZE) {

  ciphertext = IOBuf::create(capacity);

  ciphertext->reserve(nonce_len_, plaintext->length());
  ciphertext->append(plaintext->length());

  if (!init_) {
    memcpy(ciphertext->mutable_buffer(), nonce_, nonce_len_);
    set_key_stream();
    init_ = true;
  }

  int padding = counter_ % SODIUM_BLOCK_SIZE;

  if (padding) {
    plaintext->reserve(padding, 0);
    plaintext->prepend(padding);

    sodium_memzero(plaintext->mutable_data(), padding);
  }

  crypto_stream_xor_ic((uint8_t *)(ciphertext->mutable_data()),
                       (const uint8_t *)plaintext->data(),
                       (uint64_t)plaintext->length(), (const uint8_t *)nonce_,
                       counter_ / SODIUM_BLOCK_SIZE, skey_, method_);

  counter_ += plaintext->length();

  if (padding) {
    memmove(ciphertext->mutable_data(), ciphertext->data() + padding,
            ciphertext->length());
  }

  ciphertext->prepend(nonce_len_);
}

void cipher::set_key_stream() {
  // nop
}

void cipher::decrypt_aead(IOBuf *ciphertext, std::unique_ptr<IOBuf> &plaintext,
                          size_t capacity) {
  size_t salt_len = key_len_;
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
    VLOG(4) << "decrypt: salt: " << salt_len;

    memcpy(salt_, chunk_->data(), salt_len);
    chunk_->trimStart(salt_len);
    chunk_->retreat(salt_len);

    set_key_aead();

#ifndef NDEBUG
    DumpHex("DE-SALT", salt_, salt_len);
#endif

    init_ = true;
  }

  plaintext = IOBuf::create(capacity);
  while (!chunk_->empty()) {
    size_t chunk_len = 0;
    if (!chunk_decrypt_aead(plaintext.get(), chunk_.get(), &chunk_len)) {
      break;
    }

    chunk_->trimStart(chunk_len);
  }
  chunk_->retreat(chunk_->headroom());
}

void cipher::encrypt_aead(IOBuf *plaintext, std::unique_ptr<IOBuf> &ciphertext,
                          size_t capacity) {
  size_t salt_len = key_len_;

  ciphertext = IOBuf::create(capacity);
  if (!init_) {
    ciphertext->reserve(salt_len, 0);
    memcpy(ciphertext->mutable_buffer(), salt_, salt_len);
    ciphertext->prepend(salt_len);
    set_key_aead();
#ifndef NDEBUG
    DumpHex("EN-SALT", salt_, salt_len);
#endif
    init_ = true;

    VLOG(4) << "encrypt: salt: " << salt_len;
  }

  size_t clen = 2 * tag_len_ + CHUNK_SIZE_LEN + plaintext->length();

  ciphertext->reserve(0, clen);
  chunk_encrypt_aead(plaintext, ciphertext.get());

  VLOG(4) << "encrypt: current chunk: " << clen
          << " original: " << plaintext->length();
}

bool cipher::chunk_decrypt_aead(IOBuf *plaintext, const IOBuf *ciphertext,
                                size_t *consumed_len) const {
  int err;
  size_t mlen;
  size_t nlen = nonce_len_;
  size_t tlen = tag_len_;
  size_t plen = 0;

  VLOG(4) << "decrypt: current chunk: " << ciphertext->length()
          << " expected: " << (2 * tlen + CHUNK_SIZE_LEN);

  if (ciphertext->length() <= 2 * tlen + CHUNK_SIZE_LEN) {
    return false;
  }

  uint8_t len_buf[2];
  err = aead_cipher_decrypt(len_buf, &plen, ciphertext->data(),
                            CHUNK_SIZE_LEN + tlen, NULL, 0, nonce_, skey_,
                            method_);
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

  sodium_increment(nonce_, nlen);

  err = aead_cipher_decrypt(plaintext->mutable_tail(), &plen,
                            ciphertext->data() + CHUNK_SIZE_LEN + tlen,
                            mlen + tlen, NULL, 0, nonce_, skey_, method_);
  if (err) {
    return false;
  }
  DCHECK_EQ(plen, mlen);

  sodium_increment(nonce_, nlen);

  *consumed_len = chunk_len;

  plaintext->append(plen);

  return true;
}

bool cipher::chunk_encrypt_aead(const IOBuf *plaintext,
                                IOBuf *ciphertext) const {
  size_t nlen = nonce_len_;
  size_t tlen = tag_len_;

  DCHECK_LE(plaintext->length(), CHUNK_SIZE_MASK);

  int err;
  size_t clen;
  uint8_t len_buf[CHUNK_SIZE_LEN] = {};
  uint16_t t = htons(plaintext->length() & CHUNK_SIZE_MASK);
  memcpy(len_buf, &t, CHUNK_SIZE_LEN);

  clen = CHUNK_SIZE_LEN + tlen;
  err = aead_cipher_encrypt(ciphertext->mutable_tail(), &clen, len_buf,
                            CHUNK_SIZE_LEN, NULL, 0, nonce_, skey_, method_);
  if (err) {
    return false;
  }

  DCHECK_EQ(clen, CHUNK_SIZE_LEN + tlen);
  ciphertext->append(clen);

  sodium_increment(nonce_, nlen);

  clen = plaintext->length() + tlen;
  err =
      aead_cipher_encrypt(ciphertext->mutable_tail(), &clen, plaintext->data(),
                          plaintext->length(), NULL, 0, nonce_, skey_, method_);
  if (err) {
    return false;
  }

  DCHECK_EQ(clen, plaintext->length() + tlen);

  sodium_increment(nonce_, nlen);

  ciphertext->append(clen);

  return true;
}

void cipher::set_key_aead() {
  int err = crypto_hkdf(salt_, key_len_, key_, key_len_, (uint8_t *)SUBKEY_INFO,
                        sizeof(SUBKEY_INFO) - 1, skey_, key_len_);
  if (err) {
    LOG(FATAL) << "Unable to generate subkey";
  }

  memset(nonce_, 0, nonce_len_);

#ifndef NDEBUG
  DumpHex("SKEY", skey_, key_len_);
  DumpHex("NONCE", nonce_, nonce_len_);
#endif
}
