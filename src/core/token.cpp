// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/token.hpp"

#include <inttypes.h>

#include "core/pickle.hpp"
#include "core/rand_util.hpp"
#include "core/stringprintf.hpp"

#include <absl/types/optional.h>

// static
Token Token::CreateRandom() {
  Token token;

  // Use base::RandBytes instead of crypto::RandBytes, because crypto calls the
  // base version directly, and to prevent the dependency from base/ to crypto/.
  RandBytes(&token, sizeof(token));
  return token;
}

std::string Token::ToString() const {
  return StringPrintf("%016" PRIX64 "%016" PRIX64, words_[0], words_[1]);
}

void WriteTokenToPickle(Pickle* pickle, const Token& token) {
  pickle->WriteUInt64(token.high());
  pickle->WriteUInt64(token.low());
}

absl::optional<Token> ReadTokenFromPickle(PickleIterator* pickle_iterator) {
  uint64_t high;
  if (!pickle_iterator->ReadUInt64(&high))
    return absl::nullopt;

  uint64_t low;
  if (!pickle_iterator->ReadUInt64(&low))
    return absl::nullopt;

  return Token(high, low);
}
