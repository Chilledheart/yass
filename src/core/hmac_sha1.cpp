// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/hmac_sha1.hpp"

#include <cstring>

#define HASH_BLOCK_SIZE 128
#define HASH_BLOCK_SIZE_256 64

int hmac_sha1_starts(SHA_CTX* ctx,
                     unsigned char* ipad,
                     unsigned char* opad,
                     const unsigned char* key,
                     size_t keylen) {
  int ret = 0;
  uint8_t sum[SHA_DIGEST_LENGTH];
  size_t i;

  if (ctx == nullptr || ipad == nullptr || opad == nullptr)
    return -1;

  if (keylen > static_cast<size_t>(HASH_BLOCK_SIZE_256)) {
    SHA1_Init(ctx);
    SHA1_Update(ctx, key, keylen);
    SHA1_Final(sum, ctx);

    keylen = SHA_DIGEST_LENGTH;
    key = sum;
  }

  memset(ipad, 0x36, HASH_BLOCK_SIZE_256);
  memset(opad, 0x5C, HASH_BLOCK_SIZE_256);

  for (i = 0; i < keylen; i++) {
    ipad[i] = static_cast<unsigned char>(ipad[i] ^ key[i]);
    opad[i] = static_cast<unsigned char>(opad[i] ^ key[i]);
  }

  SHA1_Init(ctx);
  SHA1_Update(ctx, ipad, HASH_BLOCK_SIZE_256);

  memset(sum, 0, sizeof(sum));

  return ret;
}

int hmac_sha1_update(SHA_CTX* ctx,
                     unsigned char* ipad,
                     unsigned char* opad,
                     const unsigned char* input,
                     size_t ilen) {
  if (ctx == nullptr || ipad == nullptr || opad == nullptr)
    return -1;

  SHA1_Update(ctx, input, ilen);

  return 0;
}

int hmac_sha1_finish(SHA_CTX* ctx,
                     unsigned char* ipad,
                     unsigned char* opad,
                     unsigned char* output) {
  uint8_t tmp[SHA_DIGEST_LENGTH];

  if (ctx == nullptr || ipad == nullptr || opad == nullptr)
    return -1;

  SHA1_Final(tmp, ctx);

  SHA1_Init(ctx);
  SHA1_Update(ctx, opad, HASH_BLOCK_SIZE_256);
  SHA1_Update(ctx, tmp, SHA_DIGEST_LENGTH);
  SHA1_Final(output, ctx);

  return 0;
}

int hmac_sha1_reset(SHA_CTX* ctx, unsigned char* ipad, unsigned char* opad) {
  if (ctx == nullptr || ipad == nullptr || opad == nullptr)
    return -1;

  SHA1_Init(ctx);
  SHA1_Update(ctx, ipad, HASH_BLOCK_SIZE_256);

  return 0;
}

int hmac_sha1(const unsigned char* key,
              size_t keylen,
              const unsigned char* input,
              size_t ilen,
              unsigned char* output) {
  SHA_CTX ctx = {};
  unsigned char ipad[HASH_BLOCK_SIZE_256], opad[HASH_BLOCK_SIZE_256];
  int ret;

  SHA1_Init(&ctx);

  if ((ret = hmac_sha1_starts(&ctx, ipad, opad, key, keylen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_update(&ctx, ipad, opad, input, ilen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_finish(&ctx, ipad, opad, output)) != 0)
    goto cleanup;

cleanup:

  return ret;
}
