// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef CORE_RAND_UTIL
#define CORE_RAND_UTIL

#include <stddef.h>
#include <stdint.h>

#include <string>

// Returns a random number in range [0, UINT64_MAX]. Thread-safe.
uint64_t RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
int RandInt(int min, int max);

// Returns a random number in range [0, range).  Thread-safe.
uint64_t RandGenerator(uint64_t range);

// Returns a random double in range [0, 1). Thread-safe.
double RandDouble();

// Given input |bits|, convert with maximum precision to a double in
// the range [0, 1). Thread-safe.
double BitsToOpenEndedUnitInterval(uint64_t bits);

// Fills |output_length| bytes of |output| with random data. Thread-safe.
//
void RandBytes(void* output, size_t output_length);

// Fills a string of length |length| with random data and returns it.
// |length| should be nonzero. Thread-safe.
//
// Note that this is a variation of |RandBytes| with a different return type.
// The returned string is likely not ASCII/UTF-8. Use with care.
//
// Although implementations are required to use a cryptographically secure
// random number source, code outside of base/ that relies on this should use
// crypto::RandBytes instead to ensure the requirement is easily discoverable.
std::string RandBytesAsString(size_t length);

// An STL UniformRandomBitGenerator backed by RandUint64.
// TODO(tzik): Consider replacing this with a faster implementation.
class RandomBitGenerator {
 public:
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return UINT64_MAX; }
  result_type operator()() const { return RandUint64(); }

  RandomBitGenerator() = default;
  ~RandomBitGenerator() = default;
};

#endif  // CORE_RAND_UTIL
