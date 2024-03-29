// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
#define NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <absl/container/btree_set.h>

#pragma clang diagnostic pop

namespace quiche {

template <typename Key, typename Compare>
using QuicheSmallOrderedSetImpl = absl::btree_set<Key, Compare>;

}  // namespace quiche

#endif  // NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
