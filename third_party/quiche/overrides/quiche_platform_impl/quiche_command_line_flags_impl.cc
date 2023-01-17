// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_command_line_flags_impl.h"

#include <absl/flags/parse.h>

std::vector<std::string> QuicheParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv) {
  static_cast<void>(usage);
  auto args = absl::ParseCommandLine(argc, const_cast<char**>(argv));
  std::vector<std::string> ret;
  for (const char* arg : args)
    ret.push_back(arg);

  return ret;
}
