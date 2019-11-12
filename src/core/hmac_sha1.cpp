//
// hmac_sha1.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/hmac_sha1.hpp"

#include "core/sha1.h"
#include <cstring>

#define HASH_BLOCK_SIZE 128
#define HASH_BLOCK_SIZE_256 64

int hmac_sha1_starts(SHA1Context *ctx, unsigned char *ipad,
                     unsigned char *opad, const unsigned char *key,
                     size_t keylen) {
  int ret = 0;
  unsigned char sum[kSHA1Length];
  size_t i;

  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  if (keylen > (size_t)HASH_BLOCK_SIZE_256) {
    SHA1Init(ctx);
    SHA1Update(ctx, key, keylen);
    SHA1Final((SHA1Digest*)sum, ctx);

    keylen = kSHA1Length;
    key = sum;
  }

  memset(ipad, 0x36, HASH_BLOCK_SIZE_256);
  memset(opad, 0x5C, HASH_BLOCK_SIZE_256);

  for (i = 0; i < keylen; i++) {
    ipad[i] = (unsigned char)(ipad[i] ^ key[i]);
    opad[i] = (unsigned char)(opad[i] ^ key[i]);
  }

  SHA1Init(ctx);
  SHA1Update(ctx, ipad, HASH_BLOCK_SIZE_256);

  memset(sum, 0, sizeof(sum));

  return ret;
}

int hmac_sha1_update(SHA1Context *ctx, unsigned char *ipad,
                     unsigned char *opad, const unsigned char *input,
                     size_t ilen) {
  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  SHA1Update(ctx, input, ilen);

  return 0;
}

int hmac_sha1_finish(SHA1Context *ctx, unsigned char *ipad,
                     unsigned char *opad, unsigned char *output) {
  unsigned char tmp[kSHA1Length];

  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  SHA1Final((SHA1Digest*)tmp, ctx);

  SHA1Init(ctx);
  SHA1Update(ctx, opad, HASH_BLOCK_SIZE_256);
  SHA1Update(ctx, tmp, kSHA1Length);
  SHA1Final((SHA1Digest*)output, ctx);

  return 0;
}

int hmac_sha1_reset(SHA1Context *ctx, unsigned char *ipad,
                    unsigned char *opad) {
  if (ctx == NULL || ipad == NULL || opad == NULL)
    return -1;

  SHA1Init(ctx);
  SHA1Update(ctx, ipad, HASH_BLOCK_SIZE_256);

  return 0;
}

int hmac_sha1(const unsigned char *key, size_t keylen,
              const unsigned char *input, size_t ilen,
              unsigned char *output) {
  SHA1Context ctx;
  unsigned char ipad[HASH_BLOCK_SIZE_256], opad[HASH_BLOCK_SIZE_256];
  int ret;

  SHA1Init(&ctx);

  if ((ret = hmac_sha1_starts(&ctx, ipad, opad, key, keylen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_update(&ctx, ipad, opad, input, ilen)) != 0)
    goto cleanup;
  if ((ret = hmac_sha1_finish(&ctx, ipad, opad, output)) != 0)
    goto cleanup;

cleanup:

  return ret;
}

