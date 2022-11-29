// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_

#include <stddef.h>

#include "core/compiler_specific.hpp"

#if OS_WIN
/* Structure for scatter/gather I/O.  */
struct iovec {
  void* iov_base; /* Pointer to data.  */
  size_t iov_len; /* Length of data.  */
};
#elif OS_POSIX
#include <sys/uio.h>
#endif

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_IOVEC_IMPL_H_
