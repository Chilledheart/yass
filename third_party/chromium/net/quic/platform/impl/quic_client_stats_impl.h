// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_

#include <string>

namespace quic {

// By convention, all QUIC histograms are prefixed by "Quic.".
#define QUIC_HISTOGRAM_NAME(raw_name) "Quic." raw_name

#define QUIC_CLIENT_HISTOGRAM_ENUM_IMPL(name, sample, enum_size, docstring) \
  do {                                                                      \
  } while (0)

#define QUIC_CLIENT_HISTOGRAM_BOOL_IMPL(name, sample, docstring)            \
  do {                                                                      \
  } while (0)

#define QUIC_CLIENT_HISTOGRAM_TIMES_IMPL(name, sample, min, max, bucket_count, \
                                         docstring)                            \
  do {                                                                         \
  } while (0)

#define QUIC_CLIENT_HISTOGRAM_COUNTS_IMPL(name, sample, min, max, \
                                          bucket_count, docstring)\
  do {                                                            \
  } while (0)

inline void QuicClientSparseHistogramImpl(const std::string& name, int sample) {
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CLIENT_STATS_IMPL_H_
