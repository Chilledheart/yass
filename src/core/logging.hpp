// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#ifndef H_LOGGING
#define H_LOGGING

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iosfwd>
#include <ostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <malloc.h>
#endif

#include <absl/base/attributes.h>
#include <absl/base/optimization.h>
#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#define LOG_INFO (::absl::LogSeverity::kInfo)
#define LOG_WARNING (::absl::LogSeverity::kWarning)
#define LOG_ERROR (::absl::LogSeverity::kError)
#define LOG_FATAL (::absl::LogSeverity::kFatal)

ABSL_DECLARE_FLAG(bool, tick_counts_in_logfile_name);
ABSL_DECLARE_FLAG(bool, logtostderr);
ABSL_DECLARE_FLAG(bool, alsologtostderr);
ABSL_DECLARE_FLAG(bool, colorlogtostderr);
#if defined(__linux__) || defined(__ANDROID__)
ABSL_DECLARE_FLAG(bool, drop_log_memory);
#endif

ABSL_DECLARE_FLAG(int32_t, stderrthreshold);
ABSL_DECLARE_FLAG(bool, log_prefix);
ABSL_DECLARE_FLAG(int32_t, minloglevel);
ABSL_DECLARE_FLAG(int32_t, logbuflevel);
ABSL_DECLARE_FLAG(int32_t, logbufsecs);

ABSL_DECLARE_FLAG(int32_t, logfile_mode);
ABSL_DECLARE_FLAG(std::string, log_dir);
ABSL_DECLARE_FLAG(std::string, log_link);
ABSL_DECLARE_FLAG(int32_t, max_log_size);
ABSL_DECLARE_FLAG(bool, stop_logging_if_full_disk);
ABSL_DECLARE_FLAG(std::string, log_backtrace_at);
ABSL_DECLARE_FLAG(bool, log_utc_time);

ABSL_DECLARE_FLAG(int32_t, v);
ABSL_DECLARE_FLAG(std::string, vmodule);

using absl::LogSeverity;

#define NUM_SEVERITIES (static_cast<int>(absl::LogSeverity::kFatal) + 1)

#if defined(_MSC_VER)
#define MSVC_PUSH_DISABLE_WARNING(n) \
  __pragma(warning(push)) __pragma(warning(disable : n))
#define MSVC_POP_WARNING() __pragma(warning(pop))
#else
#define MSVC_PUSH_DISABLE_WARNING(n)
#define MSVC_POP_WARNING()
#endif

#ifndef STRIP_LOG
#define STRIP_LOG 0
#endif

// GCC can be told that a certain branch is not likely to be taken (for
// instance, a CHECK failure), and use that information in static analysis.
// Giving it this information can help it optimize for the common case in
// the absence of better information (ie. -fprofile-arcs).
//
#if ABSL_HAVE_BUILTIN(__builtin_expect) || \
    (defined(__GNUC__) && !defined(__clang__))
#define ABSL_PREDICT_BRANCH_NOT_TAKEN(x) (__builtin_expect(x, 0))
#else
#define ABSL_PREDICT_BRANCH_NOT_TAKEN(x) x
#endif

#if ABSL_HAVE_ATTRIBUTE(noinline) || (defined(__GNUC__) && !defined(__clang__))
#define ABSL_NOLINE_ATTRIBUTE __attribute__((noinline))
#else
#define ABSL_NOLINE_ATTRIBUTE
#endif

#if ABSL_HAVE_ATTRIBUTE(noreturn) || (defined(__GNUC__) && !defined(__clang__))
#define ABSL_ATTRIBUTE_NORETURN_ATTRIBUTE __attribute__((noreturn))
#else
#define ABSL_ATTRIBUTE_NORETURN_ATTRIBUTE
#endif

#if ABSL_HAVE_ATTRIBUTE(format_arg) || \
    (defined(__GNUC__) && !defined(__clang__))
#define ABSL_PRINTF_ARG_ATTRIBUTE(string_index) \
  __attribute__((format_arg(string_index)))
#else
#define ABSL_PRINTF_ARG_ATTRIBUTE(string_index)
#endif

#if STRIP_LOG == 0
#define COMPACT_LOG_INFO LogMessage(__FILE__, __LINE__)
#define LOG_TO_STRING_INFO(message) \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kInfo, message)
#else
#define COMPACT_LOG_INFO NullStream()
#define LOG_TO_STRING_INFO(message) NullStream()
#endif

#if STRIP_LOG <= 1
#define COMPACT_LOG_WARNING \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning)
#define LOG_TO_STRING_WARNING(message) \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kWarning, message)
#else
#define COMPACT_LOG_WARNING NullStream()
#define LOG_TO_STRING_WARNING(message) NullStream()
#endif

#if STRIP_LOG <= 2
#define COMPACT_LOG_ERROR \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError)
#define LOG_TO_STRING_ERROR(message) \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kError, message)
#else
#define COMPACT_LOG_ERROR NullStream()
#define LOG_TO_STRING_ERROR(message) NullStream()
#endif

#if STRIP_LOG <= 3
#define COMPACT_LOG_FATAL LogMessageFatal(__FILE__, __LINE__)
#define LOG_TO_STRING_FATAL(message) \
  LogMessage(__FILE__, __LINE__, ::absl::LogSeverity::kFatal, message)
#else
#define COMPACT_LOG_FATAL NullStreamFatal()
#define LOG_TO_STRING_FATAL(message) NullStreamFatal()
#endif

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG(severity) COMPACT_LOG_##severity.stream()

#if defined(__GNUC__)
// We emit an anonymous static int* variable at every VLOG_IS_ON(n) site.
// (Normally) the first time every VLOG_IS_ON(n) site is hit,
// we determine what variable will dynamically control logging at this site:
// it's either FLAGS_v or an appropriate internal variable
// matching the current source file that represents results of
// parsing of --vmodule flag and/or SetVLOGLevel calls.
#define VLOG_IS_ON(verboselevel)                                       \
  __extension__({                                                      \
    static absl::Flag<int32_t>* vlocal__ = NULL;                       \
    int32_t verbose_level__ = (verboselevel);                          \
    (vlocal__ == NULL                                                  \
         ? InitVLOG3__(&vlocal__, &FLAGS_v, __FILE__, verbose_level__) \
         : absl::GetFlag(*vlocal__) >= verbose_level__);               \
  })
#else
// GNU extensions not available, so we do not support --vmodule.
// Dynamic value of FLAGS_v always controls the logging level.
#define VLOG_IS_ON(verboselevel) (absl::GetFlag(FLAGS_v) >= (verboselevel))
#endif

// Set VLOG(_IS_ON) level for module_pattern to log_level.
// This lets us dynamically control what is normally set by the --vmodule flag.
// Returns the level that previously applied to module_pattern.
// NOTE: To change the log level for VLOG(_IS_ON) sites
//	 that have already executed after/during InitGoogleLogging,
//	 one needs to supply the exact --vmodule pattern that applied to them.
//       (If no --vmodule pattern applied to them
//       the value of FLAGS_v will continue to control them.)
extern int SetVLOGLevel(const char* module_pattern, int log_level);

// Various declarations needed for VLOG_IS_ON above: =========================

// Helper routine which determines the logging info for a particalur VLOG site.
//   site_flag     is the address of the site-local pointer to the controlling
//                 verbosity level
//   site_default  is the default to use for *site_flag
//   fname         is the current source file name
//   verbose_level is the argument to VLOG_IS_ON
// We will return the return value for VLOG_IS_ON
// and if possible set *site_flag appropriately.
extern bool InitVLOG3__(absl::Flag<int32_t>** site_flag,
                        absl::Flag<int32_t>* site_default,
                        const char* fname,
                        int32_t verbose_level);

#if defined(NDEBUG)
#define DCHECK_IS_ON() 0
#else
#define DCHECK_IS_ON() 1
#endif

// For DFATAL, we want to use LogMessage (as opposed to
// LogMessageFatal), to be consistent with the original behavior.
#if !DCHECK_IS_ON()
#define COMPACT_LOG_DFATAL COMPACT_LOG_ERROR
#elif STRIP_LOG <= 3
#define COMPACT_LOG_DFATAL LogMessage(__FILE__, __LINE__, FATAL)
#else
#define COMPACT_LOG_DFATAL NullStreamFatal()
#endif

// Enable/Disable old log cleaner.
void EnableLogCleaner(int overdue_days);
void DisableLogCleaner();
void SetApplicationFingerprint(const std::string& fingerprint);

class LogSink;  // defined below

// If a non-NULL sink pointer is given, we push this message to that sink.
// For LOG_TO_SINK we then do normal LOG(severity) logging as well.
// This is useful for capturing messages and passing/storing them
// somewhere more specific than the global log of the process.
// Argument types:
//   LogSink* sink;
//   LogSeverity severity;
// The cast is to disambiguate NULL arguments.
#define LOG_TO_SINK(sink, severity)                                           \
  LogMessage(__FILE__, __LINE__, LOG_##severity, static_cast<LogSink*>(sink), \
             true)                                                            \
      .stream()
#define LOG_TO_SINK_BUT_NOT_TO_LOGFILE(sink, severity)                        \
  LogMessage(__FILE__, __LINE__, LOG_##severity, static_cast<LogSink*>(sink), \
             false)                                                           \
      .stream()

// If a non-NULL string pointer is given, we write this message to that string.
// We then do normal LOG(severity) logging as well.
// This is useful for capturing messages and storing them somewhere more
// specific than the global log of the process.
// Argument types:
//   string* message;
//   LogSeverity severity;
// The cast is to disambiguate NULL arguments.
// NOTE: LOG(severity) expands to LogMessage().stream() for the specified
// severity.
#define LOG_TO_STRING(severity, message) \
  LOG_TO_STRING_##severity(static_cast<std::string*>(message)).stream()

// If a non-NULL pointer is given, we push the message onto the end
// of a vector of strings; otherwise, we report it with LOG(severity).
// This is handy for capturing messages and perhaps passing them back
// to the caller, rather than reporting them immediately.
// Argument types:
//   LogSeverity severity;
//   vector<string> *outvec;
// The cast is to disambiguate NULL arguments.
#define LOG_STRING(severity, outvec)                                       \
  LOG_TO_STRING_##severity(static_cast<std::vector<std::string>*>(outvec)) \
      .stream()

#define LOG_IF(severity, condition) \
  static_cast<void>(0),             \
      !(condition) ? (void)0 : LogMessageVoidify() & LOG(severity)

#define LOG_ASSERT(condition) \
  LOG_IF(FATAL, !(condition)) << "Assert failed: " #condition

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by DCHECK_IS_ON(), so the check will be executed regardless of
// compilation mode.  Therefore, it is safe to do things like:
//    CHECK(fp->Write(x) == 4)
#define CHECK(condition)                                     \
  LOG_IF(FATAL, ABSL_PREDICT_BRANCH_NOT_TAKEN(!(condition))) \
      << "Check failed: " #condition " "

// A container for a string pointer which can be evaluated to a bool -
// true iff the pointer is NULL.
struct CheckOpString {
  CheckOpString(std::string* str) : str_(str) {}
  // No destructor: if str_ is non-NULL, we're about to LOG(FATAL),
  // so there's no point in cleaning up str_.
  operator bool() const { return ABSL_PREDICT_BRANCH_NOT_TAKEN(str_ != NULL); }
  std::string* str_;
};

// Function is overloaded for integral types to allow static const
// integrals declared in classes and not defined to be used as arguments to
// CHECK* macros. It's not encouraged though.
template <class T>
inline const T& GetReferenceableValue(const T& t) {
  return t;
}
inline char GetReferenceableValue(char t) {
  return t;
}
inline unsigned char GetReferenceableValue(unsigned char t) {
  return t;
}
inline signed char GetReferenceableValue(signed char t) {
  return t;
}
inline short GetReferenceableValue(short t) {
  return t;
}
inline unsigned short GetReferenceableValue(unsigned short t) {
  return t;
}
inline int GetReferenceableValue(int t) {
  return t;
}
inline unsigned int GetReferenceableValue(unsigned int t) {
  return t;
}
inline long GetReferenceableValue(long t) {
  return t;
}
inline unsigned long GetReferenceableValue(unsigned long t) {
  return t;
}
inline long long GetReferenceableValue(long long t) {
  return t;
}
inline unsigned long long GetReferenceableValue(unsigned long long t) {
  return t;
}

// This is a dummy class to define the following operator.
struct DummyClassToDefineOperator {};

// Define global operator<< to declare using ::operator<<.
// This declaration will allow use to use CHECK macros for user
// defined classes which have operator<< (e.g., stl_logging.h).
inline std::ostream& operator<<(std::ostream& out,
                                const DummyClassToDefineOperator&) {
  return out;
}

// This formats a value for a failing CHECK_XX statement.  Ordinarily,
// it uses the definition for operator<<, with a few special cases below.
template <typename T>
inline void MakeCheckOpValueString(std::ostream* os, const T& v) {
  (*os) << v;
}

// Overrides for char types provide readable values for unprintable
// characters.
template <>
void MakeCheckOpValueString(std::ostream* os, const char& v);
template <>
void MakeCheckOpValueString(std::ostream* os, const signed char& v);
template <>
void MakeCheckOpValueString(std::ostream* os, const unsigned char& v);

// Provide printable value for nullptr_t
template <>
void MakeCheckOpValueString(std::ostream* os, const std::nullptr_t& v);

// Build the error message string. Specify no inlining for code size.
template <typename T1, typename T2>
std::string* MakeCheckOpString(const T1& v1,
                               const T2& v2,
                               const char* exprtext) ABSL_NOLINE_ATTRIBUTE;

// A helper class for formatting "expr (V1 vs. V2)" in a CHECK_XX
// statement.  See MakeCheckOpString for sample usage.  Other
// approaches were considered: use of a template method (e.g.,
// base::BuildCheckOpString(exprtext, base::Print<T1>, &v1,
// base::Print<T2>, &v2), however this approach has complications
// related to volatile arguments and function-pointer arguments).
class CheckOpMessageBuilder {
 public:
  // Inserts "exprtext" and " (" to the stream.
  explicit CheckOpMessageBuilder(const char* exprtext);
  // Deletes "stream_".
  ~CheckOpMessageBuilder();
  // For inserting the first variable.
  std::ostream* ForVar1() { return stream_; }
  // For inserting the second variable (adds an intermediate " vs. ").
  std::ostream* ForVar2();
  // Get the result (inserts the closing ")").
  std::string* NewString();

 private:
  std::ostringstream* stream_;
};

template <typename T1, typename T2>
std::string* MakeCheckOpString(const T1& v1,
                               const T2& v2,
                               const char* exprtext) {
  CheckOpMessageBuilder comb(exprtext);
  MakeCheckOpValueString(comb.ForVar1(), v1);
  MakeCheckOpValueString(comb.ForVar2(), v2);
  return comb.NewString();
}

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
#define DEFINE_CHECK_OP_IMPL(name, op)                                   \
  template <typename T1, typename T2>                                    \
  inline std::string* name##Impl(const T1& v1, const T2& v2,             \
                                 const char* exprtext) {                 \
    if (ABSL_PREDICT_TRUE(v1 op v2))                                     \
      return NULL;                                                       \
    else                                                                 \
      return MakeCheckOpString(v1, v2, exprtext);                        \
  }                                                                      \
  inline std::string* name##Impl(int v1, int v2, const char* exprtext) { \
    return name##Impl<int, int>(v1, v2, exprtext);                       \
  }

// We use the full name Check_EQ, Check_NE, etc. in case the file including
// base/logging.h provides its own #defines for the simpler names EQ, NE, etc.
// This happens if, for example, those are used as token names in a
// yacc grammar.
DEFINE_CHECK_OP_IMPL(Check_EQ, ==)  // Compilation error with CHECK_EQ(NULL, x)?
DEFINE_CHECK_OP_IMPL(Check_NE, !=)  // Use CHECK(x == NULL) instead.
DEFINE_CHECK_OP_IMPL(Check_LE, <=)
DEFINE_CHECK_OP_IMPL(Check_LT, <)
DEFINE_CHECK_OP_IMPL(Check_GE, >=)
DEFINE_CHECK_OP_IMPL(Check_GT, >)
#undef DEFINE_CHECK_OP_IMPL

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.

#if defined(STATIC_ANALYSIS)
// Only for static analysis tool to know that it is equivalent to assert
#define CHECK_OP_LOG(name, op, val1, val2, log) CHECK((val1)op(val2))
#elif DCHECK_IS_ON()
// In debug mode, avoid constructing CheckOpStrings if possible,
// to reduce the overhead of CHECK statments by 2x.
// Real DCHECK-heavy tests have seen 1.5x speedups.

// The meaning of "string" might be different between now and
// when this macro gets invoked (e.g., if someone is experimenting
// with other string implementations that get defined after this
// file is included).  Save the current meaning now and use it
// in the macro.
typedef std::string _Check_string;
#define CHECK_OP_LOG(name, op, val1, val2, log)                        \
  while (_Check_string* _result = Check##name##Impl(                   \
             GetReferenceableValue(val1), GetReferenceableValue(val2), \
             #val1 " " #op " " #val2))                                 \
  log(__FILE__, __LINE__, CheckOpString(_result)).stream()
#else
// In optimized mode, use CheckOpString to hint to compiler that
// the while condition is unlikely.
#define CHECK_OP_LOG(name, op, val1, val2, log)                        \
  while (CheckOpString _result = Check##name##Impl(                    \
             GetReferenceableValue(val1), GetReferenceableValue(val2), \
             #val1 " " #op " " #val2))                                 \
  log(__FILE__, __LINE__, _result).stream()
#endif  // STATIC_ANALYSIS, DCHECK_IS_ON()

#if STRIP_LOG <= 3
#define CHECK_OP(name, op, val1, val2) \
  CHECK_OP_LOG(name, op, val1, val2, LogMessageFatal)
#else
#define CHECK_OP(name, op, val1, val2) \
  CHECK_OP_LOG(name, op, val1, val2, NullStreamFatal)
#endif  // STRIP_LOG <= 3

// Equality/Inequality checks - compare two values, and log a FATAL message
// including the two values when the result is not as expected.  The values
// must have operator<<(ostream, ...) defined.
//
// You may append to the error message like so:
//   CHECK_NE(1, 2) << ": The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   CHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer
// and the other is NULL. To work around this, simply static_cast NULL to the
// type of the desired pointer.

#define CHECK_EQ(val1, val2) CHECK_OP(_EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(_NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(_LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(_LT, <, val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(_GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(_GT, >, val1, val2)

// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val) \
  CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// Helper functions for string comparisons.
// To avoid bloat, the definitions are in logging.cc.
#define DECLARE_CHECK_STROP_IMPL(func, expected)                           \
  std::string* Check##func##expected##Impl(const char* s1, const char* s2, \
                                           const char* names);
DECLARE_CHECK_STROP_IMPL(strcmp, true)
DECLARE_CHECK_STROP_IMPL(strcmp, false)
DECLARE_CHECK_STROP_IMPL(strcasecmp, true)
DECLARE_CHECK_STROP_IMPL(strcasecmp, false)
#undef DECLARE_CHECK_STROP_IMPL

// Helper macro for string comparisons.
// Don't use this macro directly in your code, use CHECK_STREQ et al below.
#define CHECK_STROP(func, op, expected, s1, s2)                            \
  while (CheckOpString _result =                                           \
             Check##func##expected##Impl((s1), (s2), #s1 " " #op " " #s2)) \
  LOG(FATAL) << *_result.str_

// String (char*) equality/inequality checks.
// CASE versions are case-insensitive.
//
// Note that "s1" and "s2" may be temporary strings which are destroyed
// by the compiler at the end of the current "full expression"
// (e.g. CHECK_STREQ(Foo().c_str(), Bar().c_str())).

#define CHECK_STREQ(s1, s2) CHECK_STROP(strcmp, ==, true, s1, s2)
#define CHECK_STRNE(s1, s2) CHECK_STROP(strcmp, !=, false, s1, s2)
#define CHECK_STRCASEEQ(s1, s2) CHECK_STROP(strcasecmp, ==, true, s1, s2)
#define CHECK_STRCASENE(s1, s2) CHECK_STROP(strcasecmp, !=, false, s1, s2)

#define CHECK_INDEX(I, A) CHECK(I < (sizeof(A) / sizeof(A[0])))
#define CHECK_BOUND(B, A) CHECK(B <= (sizeof(A) / sizeof(A[0])))

#define CHECK_DOUBLE_EQ(val1, val2)                \
  do {                                             \
    CHECK_LE((val1), (val2) + 0.000000000000001L); \
    CHECK_GE((val1), (val2)-0.000000000000001L);   \
  } while (false)

#define CHECK_NEAR(val1, val2, margin)   \
  do {                                   \
    CHECK_LE((val1), (val2) + (margin)); \
    CHECK_GE((val1), (val2) - (margin)); \
  } while (false)

// perror()..googly style!
//
// PLOG() and PLOG_IF() and PCHECK() behave exactly like their LOG* and
// CHECK equivalents with the addition that they postpend a description
// of the current state of errno to their output lines.

#define PLOG(severity) PLOG_IMPL(severity, 0).stream()

#define PLOG_IMPL(severity, counter)                           \
  ErrnoLogMessage(__FILE__, __LINE__, LOG_##severity, counter, \
                  &LogMessage::SendToLog)

#define PLOG_IF(severity, condition) \
  static_cast<void>(0),              \
      !(condition) ? (void)0 LogMessageVoidify() & PLOG(severity)

// A CHECK() macro that postpends errno if the condition is false. E.g.
//
// if (poll(fds, nfds, timeout) == -1) { PCHECK(errno == EINTR); ... }
#define PCHECK(condition)                                     \
  PLOG_IF(FATAL, ABSL_PREDICT_BRANCH_NOT_TAKEN(!(condition))) \
      << "Check failed: " #condition " "

// A CHECK() macro that lets you assert the success of a function that
// returns -1 and sets errno in case of an error. E.g.
//
// CHECK_ERR(mkdir(path, 0700));
//
// or
//
// int fd = open(filename, flags); CHECK_ERR(fd) << ": open " << filename;
#define CHECK_ERR(invocation)                                       \
  PLOG_IF(FATAL, ABSL_PREDICT_BRANCH_NOT_TAKEN((invocation) == -1)) \
      << #invocation

// Use macro expansion to create, for each use of LOG_EVERY_N(), static
// variables with the __LINE__ expansion as part of the variable name.
#define LOG_EVERY_N_VARNAME(base, line) LOG_EVERY_N_VARNAME_CONCAT(base, line)
#define LOG_EVERY_N_VARNAME_CONCAT(base, line) base##line

#define LOG_OCCURRENCES LOG_EVERY_N_VARNAME(occurrences_, __LINE__)
#define LOG_OCCURRENCES_MOD_N LOG_EVERY_N_VARNAME(occurrences_mod_n_, __LINE__)

#if ABSL_HAVE_FEATURE(thread_sanitizer) || __SANITIZE_THREAD__
#define _SANITIZE_THREAD 1
#endif

#if defined(_SANITIZE_THREAD)
#define _IFDEF_THREAD_SANITIZER(X) X
#else
#define _IFDEF_THREAD_SANITIZER(X)
#endif

#if defined(_SANITIZE_THREAD)

// We need to identify the static variables as "benign" races
// to avoid noisy reports from TSAN.
extern "C" void AnnotateBenignRaceSized(const char* file,
                                        int line,
                                        const volatile void* mem,
                                        long size,
                                        const char* description);

#endif  // _SANITIZE_THREAD

#define SOME_KIND_OF_LOG_EVERY_N(severity, n, what_to_do)                  \
  static std::atomic<int> LOG_OCCURRENCES(0), LOG_OCCURRENCES_MOD_N(0);    \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                         \
      __FILE__, __LINE__, &LOG_OCCURRENCES, sizeof(int), ""));             \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                         \
      __FILE__, __LINE__, &LOG_OCCURRENCES_MOD_N, sizeof(int), ""));       \
  ++LOG_OCCURRENCES;                                                       \
  if (++LOG_OCCURRENCES_MOD_N > n)                                         \
    LOG_OCCURRENCES_MOD_N -= n;                                            \
  if (LOG_OCCURRENCES_MOD_N == 1)                                          \
  LogMessage(__FILE__, __LINE__, ##severity, LOG_OCCURRENCES, &what_to_do) \
      .stream()

#define SOME_KIND_OF_LOG_IF_EVERY_N(severity, condition, n, what_to_do)       \
  static std::atomic<int> LOG_OCCURRENCES(0), LOG_OCCURRENCES_MOD_N(0);       \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                            \
      __FILE__, __LINE__, &LOG_OCCURRENCES, sizeof(int), ""));                \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                            \
      __FILE__, __LINE__, &LOG_OCCURRENCES_MOD_N, sizeof(int), ""));          \
  ++LOG_OCCURRENCES;                                                          \
  if (condition &&                                                            \
      ((LOG_OCCURRENCES_MOD_N = (LOG_OCCURRENCES_MOD_N + 1) % n) == (1 % n))) \
  LogMessage(__FILE__, __LINE__, ##severity, LOG_OCCURRENCES, &what_to_do)    \
      .stream()

#define SOME_KIND_OF_PLOG_EVERY_N(severity, n, what_to_do)              \
  static std::atomic<int> LOG_OCCURRENCES(0), LOG_OCCURRENCES_MOD_N(0); \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                      \
      __FILE__, __LINE__, &LOG_OCCURRENCES, sizeof(int), ""));          \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                      \
      __FILE__, __LINE__, &LOG_OCCURRENCES_MOD_N, sizeof(int), ""));    \
  ++LOG_OCCURRENCES;                                                    \
  if (++LOG_OCCURRENCES_MOD_N > n)                                      \
    LOG_OCCURRENCES_MOD_N -= n;                                         \
  if (LOG_OCCURRENCES_MOD_N == 1)                                       \
  ErrnoLogMessage(__FILE__, __LINE__, ##severity, LOG_OCCURRENCES,      \
                  &what_to_do)                                          \
      .stream()

#define SOME_KIND_OF_LOG_FIRST_N(severity, n, what_to_do)                  \
  static std::atomic<int> LOG_OCCURRENCES(0);                              \
  _IFDEF_THREAD_SANITIZER(AnnotateBenignRaceSized(                         \
      __FILE__, __LINE__, &LOG_OCCURRENCES, sizeof(int), ""));             \
  if (LOG_OCCURRENCES <= n)                                                \
    ++LOG_OCCURRENCES;                                                     \
  if (LOG_OCCURRENCES <= n)                                                \
  LogMessage(__FILE__, __LINE__, ##severity, LOG_OCCURRENCES, &what_to_do) \
      .stream()

struct CrashReason;

#define LOG_EVERY_N(severity, n) \
  SOME_KIND_OF_LOG_EVERY_N(severity, (n), LogMessage::SendToLog)

#define PLOG_EVERY_N(severity, n) \
  SOME_KIND_OF_PLOG_EVERY_N(severity, (n), LogMessage::SendToLog)

#define LOG_FIRST_N(severity, n) \
  SOME_KIND_OF_LOG_FIRST_N(severity, (n), LogMessage::SendToLog)

#define LOG_IF_EVERY_N(severity, condition, n) \
  SOME_KIND_OF_LOG_IF_EVERY_N(severity, (condition), (n), LogMessage::SendToLog)

// We want the special COUNTER value available for LOG_EVERY_X()'ed messages
enum PRIVATE_Counter { COUNTER };

#if DCHECK_IS_ON()

#define DLOG(severity) LOG(severity)
#define DVLOG(verboselevel) VLOG(verboselevel)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_EVERY_N(severity, n) LOG_EVERY_N(severity, n)
#define DLOG_IF_EVERY_N(severity, condition, n) \
  LOG_IF_EVERY_N(severity, condition, n)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)

// debug-only checking.  executed if DCHECK_IS_ON().
#define DCHECK(condition) CHECK(condition)
#define DCHECK_EQ(val1, val2) CHECK_EQ(val1, val2)
#define DCHECK_NE(val1, val2) CHECK_NE(val1, val2)
#define DCHECK_LE(val1, val2) CHECK_LE(val1, val2)
#define DCHECK_LT(val1, val2) CHECK_LT(val1, val2)
#define DCHECK_GE(val1, val2) CHECK_GE(val1, val2)
#define DCHECK_GT(val1, val2) CHECK_GT(val1, val2)
#define DCHECK_NOTNULL(val) CHECK_NOTNULL(val)
#define DCHECK_STREQ(str1, str2) CHECK_STREQ(str1, str2)
#define DCHECK_STRCASEEQ(str1, str2) CHECK_STRCASEEQ(str1, str2)
#define DCHECK_STRNE(str1, str2) CHECK_STRNE(str1, str2)
#define DCHECK_STRCASENE(str1, str2) CHECK_STRCASENE(str1, str2)

#else  // !DCHECK_IS_ON()

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

#define DLOG(severity) \
  static_cast<void>(0), true ? (void)0 : LogMessageVoidify() & LOG(severity)

#define DVLOG(verboselevel)                                 \
  static_cast<void>(0), (true || !VLOG_IS_ON(verboselevel)) \
                            ? (void)0                       \
                            : LogMessageVoidify() & LOG(INFO)

#define DLOG_IF(severity, condition) \
  static_cast<void>(0),              \
      (true || !(condition)) ? (void)0 : LogMessageVoidify() & LOG(severity)

#define DLOG_EVERY_N(severity, n) \
  static_cast<void>(0), true ? (void)0 : LogMessageVoidify() & LOG(severity)

#define DLOG_IF_EVERY_N(severity, condition, n) \
  static_cast<void>(0),                         \
      (true || !(condition)) ? (void)0 : LogMessageVoidify() & LOG(severity)

#define DLOG_ASSERT(condition) \
  static_cast<void>(0), true ? (void)0 : LOG_ASSERT(condition)

// MSVC warning C4127: conditional expression is constant
#define DCHECK(condition)         \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK(condition)

#define DCHECK_EQ(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_EQ(val1, val2)

#define DCHECK_NE(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_NE(val1, val2)

#define DCHECK_LE(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_LE(val1, val2)

#define DCHECK_LT(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_LT(val1, val2)

#define DCHECK_GE(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_GE(val1, val2)

#define DCHECK_GT(val1, val2)     \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_GT(val1, val2)

// You may see warnings in release mode if you don't use the return
// value of DCHECK_NOTNULL. Please just use DCHECK for such cases.
#define DCHECK_NOTNULL(val) (val)

#define DCHECK_STREQ(str1, str2)  \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_STREQ(str1, str2)

#define DCHECK_STRCASEEQ(str1, str2) \
  MSVC_PUSH_DISABLE_WARNING(4127)    \
  while (false)                      \
  MSVC_POP_WARNING() CHECK_STRCASEEQ(str1, str2)

#define DCHECK_STRNE(str1, str2)  \
  MSVC_PUSH_DISABLE_WARNING(4127) \
  while (false)                   \
  MSVC_POP_WARNING() CHECK_STRNE(str1, str2)

#define DCHECK_STRCASENE(str1, str2) \
  MSVC_PUSH_DISABLE_WARNING(4127)    \
  while (false)                      \
  MSVC_POP_WARNING() CHECK_STRCASENE(str1, str2)

#endif  // DCHECK_IS_ON()

// Log only in verbose mode.

#define VLOG(verboselevel) LOG_IF(INFO, VLOG_IS_ON(verboselevel))

#define VLOG_IF(verboselevel, condition) \
  LOG_IF(INFO, (condition) && VLOG_IS_ON(verboselevel))

#define VLOG_EVERY_N(verboselevel, n) \
  LOG_IF_EVERY_N(INFO, VLOG_IS_ON(verboselevel), n)

#define VLOG_IF_EVERY_N(verboselevel, condition, n) \
  LOG_IF_EVERY_N(INFO, (condition) && VLOG_IS_ON(verboselevel), n)

// LogMessage::LogStream is a std::ostream backed by this streambuf.
// This class ignores overflow and leaves two bytes at the end of the
// buffer to allow for a '\n' and '\0'.
class LogStreamBuf : public std::streambuf {
 public:
  // REQUIREMENTS: "len" must be >= 2 to account for the '\n' and '\0'.
  LogStreamBuf(char* buf, int len) { setp(buf, buf + len - 2); }

  // This effectively ignores overflow.
  int_type overflow(int_type ch) { return ch; }

  // Legacy public ostrstream method.
  size_t pcount() const { return pptr() - pbase(); }
  char* pbase() const { return std::streambuf::pbase(); }
};

//
// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class LogMessage {
 public:
  enum {
    // Passing kNoLogPrefix for the line number disables the
    // log-message prefix. Useful for using the LogMessage
    // infrastructure as a printing utility. See also the --log_prefix
    // flag for controlling the log-message prefix on an
    // application-wide basis.
    kNoLogPrefix = -1
  };

  // LogStream inherit from non-DLL-exported class (std::ostrstream)
  // and VC++ produces a warning for this situation.
  // However, MSDN says "C4275 can be ignored in Microsoft Visual C++
  // 2005 if you are deriving from a type in the Standard C++ Library"
  // http://msdn.microsoft.com/en-us/library/3tdb471s(VS.80).aspx
  // Let's just ignore the warning.
  MSVC_PUSH_DISABLE_WARNING(4275)
  class LogStream : public std::ostream {
    MSVC_POP_WARNING()
   public:
    LogStream(char* buf, int len, uint64_t ctr)
        : std::ostream(NULL), streambuf_(buf, len), ctr_(ctr), self_(this) {
      rdbuf(&streambuf_);
    }

    uint64_t ctr() const { return ctr_; }
    void set_ctr(uint64_t ctr) { ctr_ = ctr; }
    LogStream* self() const { return self_; }

    // Legacy std::streambuf methods.
    size_t pcount() const { return streambuf_.pcount(); }
    char* pbase() const { return streambuf_.pbase(); }
    char* str() const { return pbase(); }

   private:
    LogStream(const LogStream&);
    LogStream& operator=(const LogStream&);
    LogStreamBuf streambuf_;
    uint64_t ctr_;     // Counter hack (for the LOG_EVERY_X() macro)
    LogStream* self_;  // Consistency check hack
  };

 public:
  // icc 8 requires this typedef to avoid an internal compiler error.
  typedef void (LogMessage::*SendMethod)();

  LogMessage(const char* file,
             int line,
             LogSeverity severity,
             uint64_t ctr,
             SendMethod send_method);

  // Two special constructors that generate reduced amounts of code at
  // LOG call sites for common cases.

  // Used for LOG(INFO): Implied are:
  // severity = INFO, ctr = 0, send_method = &LogMessage::SendToLog.
  //
  // Using this constructor instead of the more complex constructor above
  // saves 19 bytes per call site.
  LogMessage(const char* file, int line);

  // Used for LOG(severity) where severity != INFO.  Implied
  // are: ctr = 0, send_method = &LogMessage::SendToLog
  //
  // Using this constructor instead of the more complex constructor above
  // saves 17 bytes per call site.
  LogMessage(const char* file, int line, LogSeverity severity);

  // Constructor to log this message to a specified sink (if not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SendToSinkAndLog if
  // also_send_to_log is true, send_method = &LogMessage::SendToSink otherwise.
  LogMessage(const char* file,
             int line,
             LogSeverity severity,
             LogSink* sink,
             bool also_send_to_log);

  // Constructor where we also give a vector<string> pointer
  // for storing the messages (if the pointer is not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SaveOrSendToLog.
  LogMessage(const char* file,
             int line,
             LogSeverity severity,
             std::vector<std::string>* outvec);

  // Constructor where we also give a string pointer for storing the
  // message (if the pointer is not NULL).  Implied are: ctr = 0,
  // send_method = &LogMessage::WriteToStringAndLog.
  LogMessage(const char* file,
             int line,
             LogSeverity severity,
             std::string* message);

  // A special constructor used for check failures
  LogMessage(const char* file, int line, const CheckOpString& result);

  ~LogMessage();

  // Flush a buffered message to the sink set in the constructor.  Always
  // called by the destructor, it may also be called from elsewhere if
  // needed.  Only the first call is actioned; any later ones are ignored.
  void Flush();

  // An arbitrary limit on the length of a single log message.  This
  // is so that streaming can be done more efficiently.
  static const size_t kMaxLogMessageLen;

  // Theses should not be called directly outside of logging.*,
  // only passed as SendMethod arguments to other LogMessage methods:
  void SendToLog();  // Actually dispatch to the logs

  // Call abort() or similar to perform LOG(FATAL) crash.
  static void ABSL_ATTRIBUTE_NORETURN_ATTRIBUTE Fail();

  std::ostream& stream();

  int preserved_errno() const;

  // Must be called without the log_mutex held.  (L < log_mutex)
  static int64_t num_messages(int severity);

  struct LogMessageData;

 private:
  // Fully internal SendMethod cases:
  void SendToSinkAndLog();  // Send to sink if provided and dispatch to the logs
  void SendToSink();        // Send to sink if provided, do nothing otherwise.

  // Write to string if provided and dispatch to the logs.
  void WriteToStringAndLog();

  void SaveOrSendToLog();  // Save to stringvec if provided, else to logs

  void Init(const char* file,
            int line,
            LogSeverity severity,
            void (LogMessage::*send_method)());

  // Used to fill in crash information during LOG(FATAL) failures.
  void RecordCrashReason(CrashReason* reason);

  // Counts of messages sent at each priority:
  static int64_t num_messages_[NUM_SEVERITIES];  // under log_mutex

  // We keep the data in a separate struct so that each instance of
  // LogMessage uses less stack space.
  LogMessageData* allocated_;
  LogMessageData* data_;

  friend class LogDestination;

  LogMessage(const LogMessage&);
  void operator=(const LogMessage&);
};

// This class happens to be thread-hostile because all instances share
// a single data buffer, but since it can only be created just before
// the process dies, we don't worry so much.
class LogMessageFatal : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line);
  LogMessageFatal(const char* file, int line, const CheckOpString& result);
  ABSL_ATTRIBUTE_NORETURN_ATTRIBUTE ~LogMessageFatal();
};

// A non-macro interface to the log facility; (useful
// when the logging level is not a compile-time constant).
inline void LogAtLevel(int const severity, std::string const& msg) {
  LogMessage(__FILE__, __LINE__, static_cast<LogSeverity>(severity)).stream()
      << msg;
}

// A macro alternative of LogAtLevel. New code may want to use this
// version since there are two advantages: 1. this version outputs the
// file name and the line number where this macro is put like other
// LOG macros, 2. this macro can be used as C++ stream.
#define LOG_AT_LEVEL(severity) LogMessage(__FILE__, __LINE__, severity).stream()

// Check if it's compiled in C++11 mode.
//
// GXX_EXPERIMENTAL_CXX0X is defined by gcc and clang up to at least
// gcc-4.7 and clang-3.1 (2011-12-13).  __cplusplus was defined to 1
// in gcc before 4.7 (Crosstool 16) and clang before 3.1, but is
// defined according to the language version in effect thereafter.
// Microsoft Visual Studio 14 (2015) sets __cplusplus==199711 despite
// reasonably good C++11 support, so we set LANG_CXX for it and
// newer versions (_MSC_VER >= 1900).
#if (defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L || \
     (defined(_MSC_VER) && _MSC_VER >= 1900)) &&                      \
    !defined(__UCLIBCXX_MAJOR__)
// Helper for CHECK_NOTNULL().
//
// In C++11, all cases can be handled by a single function. Since the value
// category of the argument is preserved (also for rvalue references),
// member initializer lists like the one below will compile correctly:
//
//   Foo()
//     : x_(CHECK_NOTNULL(MethodReturningUniquePtr())) {}
template <typename T>
T CheckNotNull(const char* file, int line, const char* names, T&& t) {
  if (t == nullptr) {
    LogMessageFatal(file, line, new std::string(names));
  }
  return std::forward<T>(t);
}

#else

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(const char* file, int line, const char* names, T* t) {
  if (t == NULL) {
    LogMessageFatal(file, line, new std::string(names));
  }
  return t;
}
#endif

// Allow folks to put a counter in the LOG_EVERY_X()'ed messages. This
// only works if ostream is a LogStream. If the ostream is not a
// LogStream you'll get an assert saying as much at runtime.
std::ostream& operator<<(std::ostream& os, const PRIVATE_Counter&);

// Derived class for PLOG*() above.
class ErrnoLogMessage : public LogMessage {
 public:
  ErrnoLogMessage(const char* file,
                  int line,
                  LogSeverity severity,
                  uint64_t ctr,
                  void (LogMessage::*send_method)());

  // Postpends ": strerror(errno) [errno]".
  ~ErrnoLogMessage();

 private:
  ErrnoLogMessage(const ErrnoLogMessage&);
  void operator=(const ErrnoLogMessage&);
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

class LogMessageVoidify {
 public:
  LogMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

// Flushes all log files that contains messages that are at least of
// the specified severity level.  Thread-safe.
void FlushLogFiles(LogSeverity min_severity);

// Flushes all log files that contains messages that are at least of
// the specified severity level. Thread-hostile because it ignores
// locking -- used for catastrophic failures.
void FlushLogFilesUnsafe(LogSeverity min_severity);

//
// Set the destination to which a particular severity level of log
// messages is sent.  If base_filename is "", it means "don't log this
// severity".  Thread-safe.
//
void SetLogDestination(LogSeverity severity, const char* base_filename);

//
// Set the basename of the symlink to the latest log file at a given
// severity.  If symlink_basename is empty, do not make a symlink.  If
// you don't call this function, the symlink basename is the
// invocation name of the program.  Thread-safe.
//
void SetLogSymlink(LogSeverity severity, const char* symlink_basename);

//
// Used to send logs to some other kind of destination
// Users should subclass LogSink and override send to do whatever they want.
// Implementations must be thread-safe because a shared instance will
// be called from whichever thread ran the LOG(XXX) line.
class LogSink {
 public:
  virtual ~LogSink();

  // Sink's logging logic (message_len is such as to exclude '\n' at the end).
  // This method can't use LOG() or CHECK() as logging system mutex(s) are held
  // during this call.
  virtual void send(LogSeverity severity,
                    const char* full_filename,
                    const char* base_filename,
                    int line,
                    const char* message,
                    size_t message_len,
                    uint64_t /*tick_counts*/) {
    send(severity, full_filename, base_filename, line, message, message_len);
  }
  // This send() signature is obsolete.
  // New implementations should define this in terms of
  // the above send() method.
  virtual void send(LogSeverity severity,
                    const char* full_filename,
                    const char* base_filename,
                    int line,
                    const char* message,
                    size_t message_len) = 0;

  // Redefine this to implement waiting for
  // the sink's logging logic to complete.
  // It will be called after each send() returns,
  // but before that LogMessage exits or crashes.
  // By default this function does nothing.
  // Using this function one can implement complex logic for send()
  // that itself involves logging; and do all this w/o causing deadlocks and
  // inconsistent rearrangement of log messages.
  // E.g. if a LogSink has thread-specific actions, the send() method
  // can simply add the message to a queue and wake up another thread that
  // handles real logging while itself making some LOG() calls;
  // WaitTillSent() can be implemented to wait for that logic to complete.
  // See our unittest for an example.
  virtual void WaitTillSent();

  // Returns the normal text output of the log message.
  // Can be useful to implement send().
  static std::string ToString(LogSeverity severity,
                              const char* file,
                              int line,
                              const char* message,
                              size_t message_len,
                              uint64_t tick_counts);

  // Obsolete
  static std::string ToString(LogSeverity severity,
                              const char* file,
                              int line,
                              const char* message,
                              size_t message_len) {
    return ToString(severity, file, line, message, message_len, 0);
  }
};

// Add or remove a LogSink as a consumer of logging data.  Thread-safe.
void AddLogSink(LogSink* destination);
void RemoveLogSink(LogSink* destination);

//
// Specify an "extension" added to the filename specified via
// SetLogDestination.  This applies to all severity levels.  It's
// often used to append the port we're listening on to the logfile
// name.  Thread-safe.
//
void SetLogFilenameExtension(const char* filename_extension);

//
// Make it so that all log messages of at least a particular severity
// are logged to stderr (in addition to logging to the usual log
// file(s)).  Thread-safe.
//
void SetStderrLogging(LogSeverity min_severity);

//
// Make it so that all log messages go only to stderr.  Thread-safe.
//
void LogToStderr();

const std::vector<std::string>& GetLoggingDirectories();

// Returns a set of existing temporary directories, which will be a
// subset of the directories returned by GetLogginDirectories().
// Thread-safe.
void GetExistingTempDirectories(std::vector<std::string>* list);

// Print any fatal message again -- useful to call from signal handler
// so that the last thing in the output is the fatal message.
// Thread-hostile, but a race is unlikely.
void ReprintFatalMessage();

// Truncate a log file that may be the append-only output of multiple
// processes and hence can't simply be renamed/reopened (typically a
// stdout/stderr).  If the file "path" is > "limit" bytes, copy the
// last "keep" bytes to offset 0 and truncate the rest. Since we could
// be racing with other writers, this approach has the potential to
// lose very small amounts of data. For security, only follow symlinks
// if the path is /proc/self/fd/*
void TruncateLogFile(const char* path, int64_t limit, int64_t keep);

// Truncate stdout and stderr if they are over the value specified by
// --max_log_size; keep the final 1MB.  This function has the same
// race condition as TruncateLogFile.
void TruncateStdoutStderr();

// ---------------------------------------------------------------------
// Implementation details that are not useful to most clients
// ---------------------------------------------------------------------

// A Logger is the interface used by logging modules to emit entries
// to a log.  A typical implementation will dump formatted data to a
// sequence of files.  We also provide interfaces that will forward
// the data to another thread so that the invoker never blocks.
// Implementations should be thread-safe since the logging system
// will write to them from multiple threads.

class Logger {
 public:
  virtual ~Logger();

  // Writes "message[0,message_len-1]" corresponding to an event that
  // occurred at "tick_counts".  If "force_flush" is true, the log file
  // is flushed immediately.
  //
  // The input message has already been formatted as deemed
  // appropriate by the higher level logging facility.  For example,
  // textual log messages already contain tick_counts, and the
  // file:linenumber header.
  virtual void Write(bool force_flush,
                     uint64_t tick_counts,
                     const char* message,
                     int message_len) = 0;

  // Flush any buffered messages
  virtual void Flush() = 0;

  // Get the current LOG file size.
  // The returned value is approximate since some
  // logged data may not have been flushed to disk yet.
  virtual uint32_t LogSize() = 0;
};

// Get the logger for the specified severity level.  The logger
// remains the property of the logging module and should not be
// deleted by the caller.  Thread-safe.
extern Logger* GetLogger(LogSeverity level);

// Set the logger for the specified severity level.  The logger
// becomes the property of the logging module and should not
// be deleted by the caller.  Thread-safe.
extern void SetLogger(LogSeverity level, Logger* logger);

// glibc has traditionally implemented two incompatible versions of
// strerror_r(). There is a poorly defined convention for picking the
// version that we want, but it is not clear whether it even works with
// all versions of glibc.
// So, instead, we provide this wrapper that automatically detects the
// version that is in use, and then implements POSIX semantics.
// N.B. In addition to what POSIX says, we also guarantee that "buf" will
// be set to an empty string, if this function failed. This means, in most
// cases, you do not need to check the error code and you can directly
// use the value of "buf". It will never have an undefined value.
// DEPRECATED: Use StrError(int) instead.
int posix_strerror_r(int err, char* buf, size_t len);

// A thread-safe replacement for strerror(). Returns a string describing the
// given POSIX error code.
std::string StrError(int err);

// A class for which we define operator<<, which does nothing.
class NullStream : public LogMessage::LogStream {
 public:
  // Initialize the LogStream so the messages can be written somewhere
  // (they'll never be actually displayed). This will be needed if a
  // NullStream& is implicitly converted to LogStream&, in which case
  // the overloaded NullStream::operator<< will not be invoked.
  NullStream() : LogMessage::LogStream(message_buffer_, 1, 0) {}
  NullStream(const char* /*file*/,
             int /*line*/,
             const CheckOpString& /*result*/)
      : LogMessage::LogStream(message_buffer_, 1, 0) {}
  NullStream& stream() { return *this; }

 private:
  // A very short buffer for messages (which we discard anyway). This
  // will be needed if NullStream& converted to LogStream& (e.g. as a
  // result of a conditional expression).
  char message_buffer_[2];
};

// Do nothing. This operator is inline, allowing the message to be
// compiled away. The message will not be compiled away if we do
// something like (flag ? LOG(INFO) : LOG(ERROR)) << message; when
// SKIP_LOG=WARNING. In those cases, NullStream will be implicitly
// converted to LogStream and the message will be computed and then
// quietly discarded.
template <class T>
inline NullStream& operator<<(NullStream& str, const T&) {
  return str;
}

// Similar to NullStream, but aborts the program (without stack
// trace), like LogMessageFatal.
class NullStreamFatal : public NullStream {
 public:
  NullStreamFatal() {}
  NullStreamFatal(const char* file, int line, const CheckOpString& result)
      : NullStream(file, line, result) {}
  ABSL_ATTRIBUTE_NORETURN_ATTRIBUTE ~NullStreamFatal() throw() { _exit(1); }
};

// This is similar to LOG(severity) << format... and VLOG(level) << format..,
// but
// * it is to be used ONLY by low-level modules that can't use normal LOG()
// * it is desiged to be a low-level logger that does not allocate any
//   memory and does not need any locks, hence:
// * it logs straight and ONLY to STDERR w/o buffering
// * it uses an explicit format and arguments list
// * it will silently chop off really long message strings
// Usage example:
//   RAW_LOG(ERROR, "Failed foo with %i: %s", status, error);
//   RAW_VLOG(3, "status is %i", status);
// These will print an almost standard log lines like this to stderr only:
//   E20200821 211317 file.cc:123] RAW: Failed foo with 22: bad_file
//   I20200821 211317 file.cc:142] RAW: status is 20
#define RAW_LOG(severity, ...)        \
  do {                                \
    switch (LOG_##severity) {         \
      case LOG_INFO:                  \
        RAW_LOG_INFO(__VA_ARGS__);    \
        break;                        \
      case LOG_WARNING:               \
        RAW_LOG_WARNING(__VA_ARGS__); \
        break;                        \
      case LOG_ERROR:                 \
        RAW_LOG_ERROR(__VA_ARGS__);   \
        break;                        \
      case LOG_FATAL:                 \
        RAW_LOG_FATAL(__VA_ARGS__);   \
        break;                        \
      default:                        \
        break;                        \
    }                                 \
  } while (false)

// The following STRIP_LOG testing is performed in the header file so that it's
// possible to completely compile out the logging code and the log messages.
#if STRIP_LOG == 0
#define RAW_VLOG(verboselevel, ...) \
  do {                              \
    if (VLOG_IS_ON(verboselevel)) { \
      RAW_LOG_INFO(__VA_ARGS__);    \
    }                               \
  } while (false)
#else
#define RAW_VLOG(verboselevel, ...) RawLogStub__(0, __VA_ARGS__)
#endif  // STRIP_LOG == 0

#if STRIP_LOG == 0
#define RAW_LOG_INFO(...) RawLog__(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#else
#define RAW_LOG_INFO(...) RawLogStub__(0, __VA_ARGS__)
#endif  // STRIP_LOG == 0

#if STRIP_LOG <= 1
#define RAW_LOG_WARNING(...) \
  RawLog__(LOG_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#else
#define RAW_LOG_WARNING(...) RawLogStub__(0, __VA_ARGS__)
#endif  // STRIP_LOG <= 1

#if STRIP_LOG <= 2
#define RAW_LOG_ERROR(...) RawLog__(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
#define RAW_LOG_ERROR(...) RawLogStub__(0, __VA_ARGS__)
#endif  // STRIP_LOG <= 2

#if STRIP_LOG <= 3
#define RAW_LOG_FATAL(...) RawLog__(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#else
#define RAW_LOG_FATAL(...)        \
  do {                            \
    RawLogStub__(0, __VA_ARGS__); \
    exit(1);                      \
  } while (false)
#endif  // STRIP_LOG <= 3

// Similar to CHECK(condition) << message,
// but for low-level modules: we use only RAW_LOG that does not allocate memory.
// We do not want to provide args list here to encourage this usage:
//   if (!cond)  RAW_LOG(FATAL, "foo ...", hard_to_compute_args);
// so that the args are not computed when not needed.
#define RAW_CHECK(condition, message)                             \
  do {                                                            \
    if (!(condition)) {                                           \
      RAW_LOG(FATAL, "Check %s failed: %s", #condition, message); \
    }                                                             \
  } while (false)

// Debug versions of RAW_LOG and RAW_CHECK
#ifndef NDEBUG

#define RAW_DLOG(severity, ...) RAW_LOG(severity, __VA_ARGS__)
#define RAW_DCHECK(condition, message) RAW_CHECK(condition, message)

#else  // NDEBUG

#define RAW_DLOG(severity, ...) \
  while (false)                 \
  RAW_LOG(severity, __VA_ARGS__)
#define RAW_DCHECK(condition, message) \
  while (false)                        \
  RAW_CHECK(condition, message)

#endif  // NDEBUG

// Stub log function used to work around for unused variable warnings when
// building with STRIP_LOG > 0.
static inline void RawLogStub__(int /* ignored */, ...) {}

// Helper function to implement RAW_LOG and RAW_VLOG
// Logs format... at "severity" level, reporting it
// as called from file:line.
// This does not allocate memory or acquire locks.
void RawLog__(LogSeverity severity,
              const char* file,
              int line,
              const char* format,
              ...) ABSL_PRINTF_ATTRIBUTE(4, 5);

#endif  // H_LOGGING
