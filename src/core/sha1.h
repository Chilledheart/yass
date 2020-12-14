// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_SHA1_H_
#define CORE_SHA1_H_

#include <stddef.h>

#include <array>
#include <string>

// These functions perform SHA-1 operations.

enum { kSHA1Length = 20 };  // Length in bytes of a SHA-1 hash.

// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.
std::string SHA1HashString(const std::string& str);

// Computes the SHA-1 hash of the |len| bytes in |data| and puts the hash
// in |hash|. |hash| must be kSHA1Length bytes long.
void SHA1HashBytes(const unsigned char* data,
                   size_t len,
                   unsigned char* hash);

typedef char SHA1Digest[20];

// Used for storing intermediate data during an SHA1 computation. Callers
// should not access the data.
typedef char SHA1Context[376];
// Computes the SHA1 sum of the given data buffer with the given length.
// The given 'digest' structure will be filled with the result data.
void SHA1Init(SHA1Context* context);
// For the given buffer of data, updates the given SHA context with the sum of
// the data. You can call this any number of times during the computation,
// except that SHAInit() must have been called first.
void SHA1Update(SHA1Context* context, const void* buf, size_t len);
// Finalizes the SHA operation and fills the buffer with the digest.
void SHA1Final(SHA1Digest* digest, SHA1Context* pCtx);
// Computes the SHA-1 hash of the input string |str| and returns the full
// hash.

#endif  // CORE_HASH_SHA1_H_
