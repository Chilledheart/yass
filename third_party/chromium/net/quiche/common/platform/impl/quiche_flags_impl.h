// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAGS_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAGS_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include "common/platform/api/quiche_export.h"

#define QUIC_FLAG(flag, value) \
  QUICHE_EXPORT_PRIVATE ABSL_DECLARE_FLAG(bool, flag);
#include "quic/core/quic_flags_list.h"
#undef QUIC_FLAG

#define GetQuicheFlagImpl(flag) (absl::GetFlag(FLAGS_##flag))

#define SetQuicheFlagImpl(flag, value) (absl::SetFlag(&FLAGS_##flag, value))

// ------------------------------------------------------------------------
// QUIC feature flags implementation.
// ------------------------------------------------------------------------

#define RELOADABLE_FLAG(flag) FLAGS_quic_reloadable_flag_##flag
#define RESTART_FLAG(flag) FLAGS_quic_restart_flag_##flag

#define GetQuicheReloadableFlagImpl(module, flag) \
  GetQuicheFlag(RELOADABLE_FLAG(flag))
#define SetQuicheReloadableFlagImpl(module, flag, value) \
  SetQuicheFlag(RELOADABLE_FLAG(flag), value)
#define GetQuicheRestartFlagImpl(module, flag) GetQuicheFlag(RESTART_FLAG(flag))
#define SetQuicheRestartFlagImpl(module, flag, value) \
  SetQuicheFlag(RESTART_FLAG(flag), value)

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAGS_IMPL_H_
