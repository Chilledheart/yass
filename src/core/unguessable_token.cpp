// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */


#include "core/unguessable_token.hpp"

#include <ostream>

#include "core/rand_util.hpp"

#include <openssl/mem.h>

UnguessableToken::UnguessableToken(const Token& token) : token_(token) {}

// static
UnguessableToken UnguessableToken::Create() {
  return UnguessableToken(Token::CreateRandom());
}

// static
const UnguessableToken& UnguessableToken::Null() {
  static const UnguessableToken null_token{};
  return null_token;
}

// static
UnguessableToken UnguessableToken::Deserialize(uint64_t high, uint64_t low) {
  // Receiving a zeroed out UnguessableToken from another process means that it
  // was never initialized via Create(). The real check for this is in the
  // StructTraits in mojo/public/cpp/base/unguessable_token_mojom_traits.cc
  // where a zero-ed out token will fail to deserialize. This DCHECK is a
  // backup check.
  DCHECK(!(high == 0 && low == 0));
  return UnguessableToken(Token{high, low});
}

bool UnguessableToken::operator==(const UnguessableToken& other) const {
  auto bytes = token_.data();
  auto other_bytes = other.token_.data();
  return CRYPTO_memcmp(bytes, other_bytes, token_.size()) == 0;
}

std::ostream& operator<<(std::ostream& out, const UnguessableToken& token) {
  return out << "(" << token.ToString() << ")";
}
