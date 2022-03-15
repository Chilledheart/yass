// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
#define NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_

#include <string>
#include <absl/flags/declare.h>
#include <absl/flags/flag.h>
#include <absl/types/optional.h>

#include "common/platform/api/quiche_flags.h"
#include "quic/platform/api/quic_export.h"

#define QUIC_PROTOCOL_FLAG(type, flag, ...) \
  QUIC_EXPORT_PRIVATE ABSL_DECLARE_FLAG(type, FLAGS_##flag);
#include "quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG

#endif  // NET_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
