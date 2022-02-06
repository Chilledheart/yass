// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_STACK_TRACE_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_STACK_TRACE_IMPL_H_

#include <absl/debugging/stacktrace.h>
#include <absl/debugging/symbolize.h>
#include <absl/strings/str_cat.h>

#include "core/stringprintf.hpp"

namespace quic {

namespace {
// Print a program counter and its symbol name.
static void DumpPCAndSymbol(std::string* str,
                            void* pc,
                            const char* const prefix) {
  static const int kPrintfPointerFieldWidth = 2 + 2 * sizeof(void*);
  char tmp[1024];
  const char* symbol = "(unknown)";
  // Symbolizes the previous address of pc because pc may be in the
  // next function.  The overrun happens when the function ends with
  // a call to a function annotated noreturn (e.g. CHECK).
  if (absl::Symbolize(reinterpret_cast<char*>(pc) - 1, tmp, sizeof(tmp))) {
    symbol = tmp;
  }
  *str = StringPrintf("%s@ %*p  %s\n", prefix, kPrintfPointerFieldWidth, pc,
                      symbol);
}
} // namespace

inline std::string QuicStackTraceImpl() {
  void *stacks[32];
  std::string result;

  int depth = absl::GetStackTrace(stacks, sizeof(stacks) / sizeof(stacks[0]),
                                  1);
  for (uint32_t i = 0; i < sizeof(stacks) / sizeof(stacks[0]); ++i) {
    std::string line;
    DumpPCAndSymbol(&line, stacks[i], "    ");
    result = absl::StrCat(result, line);
  }
  return result;
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_STACK_TRACE_IMPL_H_
