// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */
#ifndef H_NET_HMAC_SHA1
#define H_NET_HMAC_SHA1

#include <openssl/sha.h>

#define HASH_BLOCK_SIZE 128
#define HASH_BLOCK_SIZE_256 64

#define OUTPUT_SIZE_SHA1 SHA_DIGEST_LENGTH

namespace net {

int hmac_sha1_starts(SHA_CTX* ctx, unsigned char* ipad, unsigned char* opad, const unsigned char* key, size_t keylen);

int hmac_sha1_update(SHA_CTX* ctx, unsigned char* ipad, unsigned char* opad, const unsigned char* input, size_t ilen);

int hmac_sha1_finish(SHA_CTX* ctx, unsigned char* ipad, unsigned char* opad, unsigned char* output);

int hmac_sha1_reset(SHA_CTX* ctx, unsigned char* ipad, unsigned char* opad);

int hmac_sha1(const unsigned char* key, size_t keylen, const unsigned char* input, size_t ilen, unsigned char* output);

}  // namespace net

#endif  // H_NET_HMAC_SHA1
