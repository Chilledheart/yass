// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_command_line_flags_impl.h"

#include <absl/flags/parse.h>
#include <absl/flags/usage.h>

std::vector<std::string> QuicheParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv) {
  static_cast<void>(usage);
  auto parsed = absl::ParseCommandLine(argc, const_cast<char**>(argv));
  std::vector<std::string> result;
  result.reserve(parsed.size());
  // Remove the first argument, which is the name of the binary.
  for (size_t i = 1; i < parsed.size(); i++) {
    result.push_back(std::string(parsed[i]));
  }
  return result;
}

void QuichePrintCommandLineFlagHelpImpl(const char* usage) {
  static_cast<void>(usage);
  std::cerr << absl::ProgramUsageMessage() << std::endl;
}
