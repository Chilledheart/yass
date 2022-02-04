// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_flags_impl.h"

#include <stdio.h>
#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <set>
#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "quic/platform/api/quic_logging.h"

#define DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE(type, flag, value, doc) \
  ABSL_FLAG(type, FLAGS_##flag, value, doc);

#define DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES(type, flag, internal_value, \
                                             external_value, doc)        \
  ABSL_FLAG(type, FLAGS_##flag, external_value, doc);

// Preprocessor macros can only have one definition.
// Select the right macro based on the number of arguments.
#define GET_6TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#define QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(...)                    \
  GET_6TH_ARG(__VA_ARGS__, DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES, \
              DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE)
#define QUIC_PROTOCOL_FLAG(...) \
  QUIC_PROTOCOL_FLAG_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#include "quic/core/quic_protocol_flags_list.h"

#undef QUIC_PROTOCOL_FLAG
#undef QUIC_PROTOCOL_FLAG_MACRO_CHOOSER
#undef GET_6TH_ARG
#undef DEFINE_QUIC_PROTOCOL_FLAG_TWO_VALUES
#undef DEFINE_QUIC_PROTOCOL_FLAG_SINGLE_VALUE

std::vector<std::string> QuicParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv) {
  auto args = absl::ParseCommandLine(argc, const_cast<char**>(argv));
  std::vector<std::string> result;
  result.reserve(args.size());
  for (char* arg : args) {
    result.emplace_back(arg);
  }

  return result;
}

void QuicPrintCommandLineFlagHelpImpl(const char* usage) {
  fprintf(stderr, "%s", usage);
}
