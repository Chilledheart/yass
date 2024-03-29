// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_
#define NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_

#include <cstdint>

#include "absl/types/optional.h"

namespace quiche {

absl::optional<int64_t> QuicheUtcDateTimeToUnixSecondsImpl(int year,
                                                           int month,
                                                           int day,
                                                           int hour,
                                                           int minute,
                                                           int second);

}  // namespace quiche

#endif  // NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TIME_UTILS_IMPL_H_
