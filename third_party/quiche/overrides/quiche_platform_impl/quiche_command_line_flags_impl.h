// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_

#include <absl/flags/flag.h>
#include <string>
#include <vector>

// ------------------------------------------------------------------------
// DEFINE_QUICHE_COMMAND_LINE_FLAG implementation.
// ------------------------------------------------------------------------

#define DEFINE_QUICHE_COMMAND_LINE_FLAG_IMPL(type, name, default_value, help) \
  ABSL_FLAG(type, name, default_value, help)

namespace quiche {

template <typename T>
T GetQuicheCommandLineFlag(const absl::Flag<T>& flag) {
  return absl::GetFlag(flag);
}

std::vector<std::string> QuicheParseCommandLineFlagsImpl(
    const char* usage, int argc, const char* const* argv,
    bool parse_only = false);

}  // namespace quiche

void QuichePrintCommandLineFlagHelpImpl(const char* usage);

template <typename T>
T GetQuicheFlagImplImpl(const absl::Flag<T>& flag) {
  return absl::GetFlag(flag);
}

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_
