// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
#define QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <absl/container/btree_set.h>

#pragma clang diagnostic pop

namespace quiche {

template <typename Key, typename Compare>
using QuicheSmallOrderedSetImpl = absl::btree_set<Key, Compare,
      std::allocator<Key>>;

}  // namespace quiche

#endif  // QUICHE_COMMON_PLATFORM_DEFAULT_QUICHE_PLATFORM_IMPL_QUICHE_CONTAINERS_IMPL_H_
