// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_CHECK_H_
#define POLYFILLS_BASE_CHECK_H_

#include <iosfwd>

#include "polyfills/base/base_export.h"
#include "polyfills/base/logging.h"

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/immediate_crash.h"

namespace gurl_base {
namespace logging {

// This header defines the CHECK, DCHECK, and DPCHECK macros.
//
// CHECK dies with a fatal error if its condition is not true. It is not
// controlled by NDEBUG, so the check will be executed regardless of compilation
// mode.
//
// DCHECK, the "debug mode" check, is enabled depending on NDEBUG and
// DCHECK_ALWAYS_ON, and its severity depends on DCHECK_IS_CONFIGURABLE.
//
// (D)PCHECK is like (D)CHECK, but includes the system error code (c.f.
// perror(3)).
//
// Additional information can be streamed to these macros and will be included
// in the log output if the condition doesn't hold (you may need to include
// <ostream>):
//
//   CHECK(condition) << "Additional info.";
//
// The condition is evaluated exactly once. Even in build modes where e.g.
// DCHECK is disabled, the condition and any stream arguments are still
// referenced to avoid warnings about unused variables and functions.
//
// For the (D)CHECK_EQ, etc. macros, see base/check_op.h. However, that header
// is *significantly* larger than check.h, so try to avoid including it in
// header files.

// Class used to explicitly ignore an ostream, and optionally a boolean value.
class VoidifyStream {
 public:
  VoidifyStream() = default;
  explicit VoidifyStream(bool /*ignored*/) {}

  // This operator has lower precedence than << but higher than ?:
  void operator&(std::ostream&) {}
};

// Helper macro which avoids evaluating the arguents to a stream if the
// condition is false.
#define LAZY_CHECK_STREAM(stream, condition) !(condition) ? (void)0 : ::gurl_base::logging::VoidifyStream() & (stream)

// Macro which uses but does not evaluate expr and any stream parameters.
#define EAT_CHECK_STREAM_PARAMS(expr) true ? (void)0 : ::gurl_base::logging::VoidifyStream(expr) & (*::gurl_base::logging::g_swallow_stream)

extern std::ostream* g_swallow_stream;

class CheckOpResult;
class LogMessage;

// Class used for raising a check error upon destruction.
class CheckError {
 public:
  static CheckError Check(const char* file, int line, const char* condition);
  static CheckError CheckOp(const char* file, int line, ::gurl_base::logging::CheckOpResult* result);

  static CheckError DCheck(const char* file, int line, const char* condition);
  static CheckError DCheckOp(const char* file, int line, ::gurl_base::logging::CheckOpResult* result);

  static CheckError PCheck(const char* file, int line, const char* condition);
  static CheckError PCheck(const char* file, int line);

  static CheckError DPCheck(const char* file, int line, const char* condition);

  static CheckError NotImplemented(const char* file, int line, const char* function);

  // Stream for adding optional details to the error message.
  std::ostream& stream();

  NOINLINE NOT_TAIL_CALLED ~CheckError();

  CheckError(const CheckError& other) = delete;
  CheckError& operator=(const CheckError& other) = delete;
  CheckError(CheckError&& other) = default;
  CheckError& operator=(CheckError&& other) = default;

 private:
  explicit CheckError(LogMessage* log_message);

  LogMessage* log_message_;
};

#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()

// Discard log strings to reduce code bloat.
//
// This is not calling BreakDebugger since this is called frequently, and
// calling an out-of-line function instead of a noreturn inline macro prevents
// compiler optimizations.
#define CHECK(condition) UNLIKELY(!(condition)) ? ::gurl_base::ImmediateCrash() : EAT_CHECK_STREAM_PARAMS()

#define PCHECK(condition) \
  LAZY_CHECK_STREAM(::gurl_base::logging::CheckError::PCheck(__FILE__, __LINE__).stream(), UNLIKELY(!(condition)))

#else

#define CHECK(condition)                                                                \
  LAZY_CHECK_STREAM(::gurl_base::logging::CheckError::Check(__FILE__, __LINE__, #condition).stream(), \
                    !ANALYZER_ASSUME_TRUE(condition))

#define PCHECK(condition)                                                                \
  LAZY_CHECK_STREAM(::gurl_base::logging::CheckError::PCheck(__FILE__, __LINE__, #condition).stream(), \
                    !ANALYZER_ASSUME_TRUE(condition))

#endif

#if DCHECK_IS_ON()

#define DCHECK(condition)                                                                \
  LAZY_CHECK_STREAM(::gurl_base::logging::CheckError::DCheck(__FILE__, __LINE__, #condition).stream(), \
                    !ANALYZER_ASSUME_TRUE(condition))

#define DPCHECK(condition)                                                                \
  LAZY_CHECK_STREAM(::gurl_base::logging::CheckError::DPCheck(__FILE__, __LINE__, #condition).stream(), \
                    !ANALYZER_ASSUME_TRUE(condition))

#else

#define DCHECK(condition) EAT_CHECK_STREAM_PARAMS(!(condition))
#define DPCHECK(condition) EAT_CHECK_STREAM_PARAMS(!(condition))

#endif

// Async signal safe checking mechanism.
void RawCheck(const char* message);
void RawError(const char* message);
#define RAW_CHECK(condition)                      \
  do {                                            \
    if (!(condition))                             \
      RawCheck("Check failed: " #condition "\n"); \
  } while (0)

#define NOTREACHED() DCHECK(false)

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE can be used.
#if DCHECK_IS_ON()
#define NOTIMPLEMENTED() CheckError::NotImplemented(__FILE__, __LINE__, __PRETTY_FUNCTION__).stream()
#else
#define NOTIMPLEMENTED() EAT_CHECK_STREAM_PARAMS()
#endif

#define NOTIMPLEMENTED_LOG_ONCE()    \
  {                                  \
    static bool logged_once = false; \
    if (!logged_once) {              \
      NOTIMPLEMENTED();              \
      logged_once = true;            \
    }                                \
  }                                  \
  EAT_CHECK_STREAM_PARAMS()

}  // namespace logging
}  // namespace gurl_base

#define GURL_CHECK(val) CHECK(val)
#define GURL_DCHECK(val) DCHECK(val)
#define GURL_NOTREACHED() NOTREACHED()

#endif /* POLYFILLS_BASE_CHECK_H_ */
