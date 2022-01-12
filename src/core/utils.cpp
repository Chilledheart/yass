// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

absl::StatusOr<int32_t> StringToInteger(absl::string_view value) {
  long result = 0;
  char* endptr = nullptr;
  result = strtol(value.data(), &endptr, 10);
  if (result > INT32_MAX || (errno == ERANGE && result == LONG_MAX)) {
    return absl::InvalidArgumentError("overflow");
  } else if (result < INT_MIN || (errno == ERANGE && result == LONG_MIN)) {
    return absl::InvalidArgumentError("underflow");
  } else if (*endptr != '\0') {
    return absl::InvalidArgumentError("bad integer");
  }
  return static_cast<int32_t>(result);
}

#ifdef _WIN32
const char kSeparators[] = "/\\";
#else
const char kSeparators[] = "/";
#endif
