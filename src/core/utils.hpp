// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS

#include <absl/status/statusor.h>
#include <absl/strings/string_view.h>
#include <cstdint>

extern uint64_t GetMonotonicTime();
extern absl::StatusOr<int32_t> StringToInteger(absl::string_view value);

#define NS_PER_SECOND (1000 * 1000 * 1000)

#endif  // YASS_UTILS
