// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_TOKEN_H_
#define CORE_TOKEN_H_

#include <stdint.h>

#include <string>
#include <tuple>

#include "core/hash.hpp"

#include <absl/types/optional.h>

// A Token is a randomly chosen 128-bit integer. This class supports generation
// from a cryptographically strong random source, or constexpr construction over
// fixed values (e.g. to store a pre-generated constant value). Tokens are
// similar in spirit and purpose to UUIDs, without many of the constraints and
// expectations (such as byte layout and string representation) clasically
// associated with UUIDs.
class Token {
 public:
  // Constructs a zero Token.
  constexpr Token() = default;

  // Constructs a Token with |high| and |low| as its contents.
  constexpr Token(uint64_t high, uint64_t low) : words_{high, low} {}

  constexpr Token(const Token&) = default;
  constexpr Token& operator=(const Token&) = default;
  constexpr Token(Token&&) noexcept = default;
  constexpr Token& operator=(Token&&) = default;

  // Constructs a new Token with random |high| and |low| values taken from a
  // cryptographically strong random source.
  static Token CreateRandom();

  // The high and low 64 bits of this Token.
  constexpr uint64_t high() const { return words_[0]; }
  constexpr uint64_t low() const { return words_[1]; }

  constexpr bool is_zero() const { return words_[0] == 0 && words_[1] == 0; }

  const uint64_t *data() const {
    return words_;
  }

  uint64_t *data() {
    return words_;
  }

  size_t size() const {
    return sizeof(words_);
  }

  constexpr bool operator==(const Token& other) const {
    return words_[0] == other.words_[0] && words_[1] == other.words_[1];
  }

  constexpr bool operator!=(const Token& other) const {
    return !(*this == other);
  }

  constexpr bool operator<(const Token& other) const {
    return std::tie(words_[0], words_[1]) <
           std::tie(other.words_[0], other.words_[1]);
  }

  // Generates a string representation of this Token useful for e.g. logging.
  std::string ToString() const;

 private:
  // Note: Two uint64_t are used instead of uint8_t[16] in order to have a
  // simpler implementation, paricularly for |ToString()|, |is_zero()|, and
  // constexpr value construction.

  uint64_t words_[2] = {0, 0};
};

// For use in std::unordered_map.
struct TokenHash {
  size_t operator()(const Token& token) const {
    return HashInts64(token.high(), token.low());
  }
};

class Pickle;
class PickleIterator;

// For serializing and deserializing Token values.
void WriteTokenToPickle(Pickle* pickle, const Token& token);
absl::optional<Token> ReadTokenFromPickle(
    PickleIterator* pickle_iterator);

#endif  // CORE_TOKEN_H_
