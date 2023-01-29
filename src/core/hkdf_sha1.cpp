// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Chilledheart  */
/* HKDF-Extract + HKDF-Expand */

#include "core/hkdf_sha1.hpp"

#include <cstring>

#include "core/compiler_specific.hpp"
#include "core/hmac_sha1.hpp"

#define MAX_MD_SIZE SHA512_DIGEST_LENGTH /* longest known is SHA512 */
#define MD_MAX_SIZE_256 \
  SHA256_DIGEST_LENGTH /* longest known is SHA256 or less */

int crypto_hkdf(const unsigned char* salt,
                int salt_len,
                const unsigned char* ikm,
                int ikm_len,
                const unsigned char* info,
                int info_len,
                unsigned char* okm,
                int okm_len) {
  unsigned char prk[MD_MAX_SIZE_256];

  return crypto_hkdf_extract(salt, salt_len, ikm, ikm_len, prk) ||
         crypto_hkdf_expand(prk, OUTPUT_SIZE_SHA1, info, info_len, okm,
                            okm_len);
}

/* HKDF-Extract(salt, IKM) -> PRK */
int crypto_hkdf_extract(const unsigned char* salt,
                        int salt_len,
                        const unsigned char* ikm,
                        int ikm_len,
                        unsigned char* prk) {
  int hash_len;
  unsigned char null_salt[MD_MAX_SIZE_256] = {};

  if (salt_len < 0) {
    return -1;
  }

  hash_len = OUTPUT_SIZE_SHA1;

  if (salt == nullptr) {
    salt = null_salt;
    salt_len = hash_len;
  }

  return hmac_sha1(salt, salt_len, ikm, ikm_len, prk);
}

/* HKDF-Expand(PRK, info, L) -> OKM */
int crypto_hkdf_expand(const unsigned char* prk,
                       int prk_len,
                       const unsigned char* info,
                       int info_len,
                       unsigned char* okm,
                       int okm_len) {
  int hash_len;
  int N;
  int T_len = 0, where = 0, i, ret;
  SHA_CTX ctx;
  unsigned char ipad[HASH_BLOCK_SIZE_256], opad[HASH_BLOCK_SIZE_256];
  unsigned char T[MD_MAX_SIZE_256];
  MSAN_UNPOISON(ipad, HASH_BLOCK_SIZE_256);
  MSAN_UNPOISON(opad, HASH_BLOCK_SIZE_256);
  MSAN_UNPOISON(T, MD_MAX_SIZE_256);

  if (info_len < 0 || okm_len < 0 || okm == nullptr) {
    return -1;
  }

  hash_len = OUTPUT_SIZE_SHA1;

  if (prk_len < hash_len) {
    return -1;
  }

  if (info == nullptr) {
    info = reinterpret_cast<const unsigned char*>("");
  }

  N = okm_len / hash_len;

  if ((okm_len % hash_len) != 0) {
    N++;
  }

  if (N > 255) {
    return -1;
  }

  SHA1_Init(&ctx);

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
