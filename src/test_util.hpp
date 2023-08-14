// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "base/containers/span.h"

#ifndef _TEST_UTIL_H
#define _TEST_UTIL_H

#include <cstring>
#include <ostream>

namespace testing {

using gurl_base::span;

// hexdump writes |msg| to |fp| followed by the hex encoding of |len| bytes
// from |in|.
void hexdump(FILE *fp, const char *msg, const void *in, size_t len);

// Bytes is a wrapper over a byte slice which may be compared for equality. This
// allows it to be used in EXPECT_EQ macros.
struct Bytes {
  Bytes(const uint8_t *data_arg, size_t len_arg)
      : span_(data_arg, len_arg) {}
  Bytes(const char *data_arg, size_t len_arg)
      : span_(reinterpret_cast<const uint8_t *>(data_arg), len_arg) {}

  Bytes(const Bytes&) = default;
  Bytes& operator=(const Bytes&) = default;

  explicit Bytes(const char *str)
      : span_(reinterpret_cast<const uint8_t *>(str), strlen(str)) {}
  explicit Bytes(const std::string &str)
      : span_(reinterpret_cast<const uint8_t *>(str.data()), str.size()) {}
  explicit Bytes(span<const uint8_t> span)
      : span_(span) {}

  span<const uint8_t> span_;
};

// DecodeHex decodes |in| from hexadecimal and writes the output to |out|. It
// returns true on success and false if |in| is not a valid hexadecimal byte
// string.
bool DecodeHex(std::vector<uint8_t> *out, const std::string &in);

// EncodeHex returns |in| encoded in hexadecimal.
std::string EncodeHex(span<const uint8_t> in);

inline bool operator==(const ::testing::Bytes &a, const ::testing::Bytes &b) {
  return a.span_.size_bytes() == b.span_.size_bytes() &&
    memcmp(a.span_.data(), b.span_.data(), a.span_.size_bytes()) == 0;
}

inline bool operator!=(const ::testing::Bytes& a, const ::testing::Bytes& b) {
  return !(a == b);
}

extern std::ostream &operator<<(std::ostream &os, ::testing::Bytes in);

}  // namespace testing

#endif  // _TEST_UTIL_H
