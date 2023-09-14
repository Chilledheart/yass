// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/check_op.hpp"

// check_op.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#pragma clang max_tokens_here 244000

#include <inttypes.h>
#include <string.h>

#include <cstdio>
#include <sstream>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace yass {

char* CheckOpValueStr(int v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%u", v);
  return strdup(buf);
}

char* CheckOpValueStr(long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%ld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lu", v);
  return strdup(buf);
}

char* CheckOpValueStr(long long v) {
  char buf[50];
#ifdef _WIN32
  snprintf(buf, sizeof(buf), "%" PRId64, v);
#else
  snprintf(buf, sizeof(buf), "%lld", v);
#endif
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long long v) {
  char buf[50];
#ifdef _WIN32
  snprintf(buf, sizeof(buf), "%" PRIu64, v);
#else
  snprintf(buf, sizeof(buf), "%llu", v);
#endif
  return strdup(buf);
}

char* CheckOpValueStr(const void* v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%p", v);
  return strdup(buf);
}

char* CheckOpValueStr(std::nullptr_t /*v*/) {
  return strdup("nullptr");
}

char* CheckOpValueStr(const std::string& v) {
  return strdup(v.c_str());
}

char* CheckOpValueStr(double v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%.6lf", v);
  return strdup(buf);
}

char* StreamValToStr(const void* v,
                     void (*stream_func)(std::ostream&, const void*)) {
  std::ostringstream ss;
  stream_func(ss, v);
  return strdup(ss.str().c_str());
}

CheckOpResult::CheckOpResult(const char* expr_str, char* v1_str, char* v2_str) {
  std::ostringstream ss;
  ss << expr_str << " (" << v1_str << " vs. " << v2_str << ")";
  message_ = strdup(ss.str().c_str());
  free(v1_str);
  free(v2_str);
}

} // namespace yass
