// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_LOGGING_H_
#define POLYFILLS_BASE_LOGGING_H_

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iosfwd>
#include <ostream>
#include <sstream>
#include <string>

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/utf_ostream_operators.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <malloc.h>
#endif

namespace gurl_base {
namespace logging {

using LogSeverity = int;
constexpr LogSeverity LOGGING_VERBOSE = -1;  // This is level 1 verbosity
// Note: the log severities are used to index into the array of names,
// see log_severity_names.
constexpr LogSeverity LOGGING_INFO = 0;
constexpr LogSeverity LOGGING_WARNING = 1;
constexpr LogSeverity LOGGING_ERROR = 2;
constexpr LogSeverity LOGGING_FATAL = 3;
constexpr LogSeverity LOGGING_NUM_SEVERITIES = 4;

// LOGGING_DFATAL is LOGGING_FATAL in DCHECK-enabled builds, ERROR in normal
// mode.
#if DCHECK_IS_ON()
constexpr LogSeverity LOGGING_DFATAL = LOGGING_FATAL;
#else
constexpr LogSeverity LOGGING_DFATAL = LOGGING_ERROR;
#endif

// This block duplicates the above entries to facilitate incremental conversion
// from LOG_FOO to LOGGING_FOO.
// TODO(thestig): Convert existing users to LOGGING_FOO and remove this block.
constexpr LogSeverity LOG_VERBOSE = LOGGING_VERBOSE;
constexpr LogSeverity LOG_INFO = LOGGING_INFO;
constexpr LogSeverity LOG_WARNING = LOGGING_WARNING;
constexpr LogSeverity LOG_ERROR = LOGGING_ERROR;
constexpr LogSeverity LOG_FATAL = LOGGING_FATAL;
constexpr LogSeverity LOG_DFATAL = LOGGING_DFATAL;

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_LOGGING_EX_INFO(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_INFO, ##__VA_ARGS__)
#define COMPACT_LOGGING_EX_WARNING(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_WARNING, ##__VA_ARGS__)
#define COMPACT_LOGGING_EX_ERROR(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_ERROR, ##__VA_ARGS__)
#define COMPACT_LOGGING_EX_FATAL(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_FATAL, ##__VA_ARGS__)
#define COMPACT_LOGGING_EX_DFATAL(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_DFATAL, ##__VA_ARGS__)
#define COMPACT_LOGGING_EX_DCHECK(ClassName, ...) ::gurl_base::logging::ClassName(__FILE__, __LINE__, ::gurl_base::logging::LOGGING_DCHECK, ##__VA_ARGS__)

#define COMPACT_LOGGING_INFO COMPACT_LOGGING_EX_INFO(LogMessage)
#define COMPACT_LOGGING_WARNING COMPACT_LOGGING_EX_WARNING(LogMessage)
#define COMPACT_LOGGING_ERROR COMPACT_LOGGING_EX_ERROR(LogMessage)
#define COMPACT_LOGGING_FATAL COMPACT_LOGGING_EX_FATAL(LogMessage)
#define COMPACT_LOGGING_DFATAL COMPACT_LOGGING_EX_DFATAL(LogMessage)
#define COMPACT_LOGGING_DCHECK COMPACT_LOGGING_EX_DCHECK(LogMessage)

#if BUILDFLAG(IS_WIN)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets
// substituted with 0, and it expands to COMPACT_LOGGING_0. To allow us
// to keep using this syntax, we define this macro to do the same thing
// as COMPACT_LOGGING_ERROR, and also define ERROR the same way that
// the Windows SDK does for consistency.
#define ERROR 0
#define COMPACT_LOGGING_EX_0(ClassName, ...) COMPACT_LOGGING_EX_ERROR(ClassName, ##__VA_ARGS__)
#define COMPACT_LOGGING_0 COMPACT_LOGGING_ERROR
// Needed for LOG_IS_ON(ERROR).
constexpr LogSeverity LOGGING_0 = LOGGING_ERROR;
#endif

// As special cases, we can assume that LOG_IS_ON(FATAL) always holds. Also,
// LOG_IS_ON(DFATAL) always holds in debug mode. In particular, CHECK()s will
// always fire if they fail.
#define LOG_IS_ON(severity) (::gurl_base::logging::ShouldCreateLogMessage(::gurl_base::logging::LOGGING_##severity))

#if defined(__GNUC__)
// We emit an anonymous static int* variable at every VLOG_IS_ON(n) site.
// (Normally) the first time every VLOG_IS_ON(n) site is hit,
// we determine what variable will dynamically control logging at this site:
// it's either FLAGS_v or an appropriate internal variable
// matching the current source file that represents results of
// parsing of --vmodule flag and/or SetVLOGLevel calls.
#define VLOG_IS_ON(verboselevel)                                                            \
  __extension__({                                                                           \
    static absl::Flag<int32_t>* vlocal__ = NULL;                                            \
    int32_t verbose_level__ = (verboselevel);                                               \
    (vlocal__ == NULL ? ::gurl_base::logging::InitVLOG3__(&vlocal__, &FLAGS_v, __FILE__, verbose_level__) \
                      : absl::GetFlag(*vlocal__) >= verbose_level__);                       \
  })
#else
// GNU extensions not available, so we do not support --vmodule.
// Dynamic value of FLAGS_v always controls the logging level.
#define VLOG_IS_ON(verboselevel) (absl::GetFlag(FLAGS_v) >= (verboselevel))
#endif

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold. Condition is evaluated once and only once.
#define LAZY_STREAM(stream, condition) !(condition) ? (void)0 : ::gurl_base::logging::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token COMPACT_LOGGING_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) COMPACT_LOGGING_##severity.stream()

#define LOG(severity) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

// The VLOG macros log with negative verbosities.
#define VLOG_STREAM(verbose_level) ::gurl_base::logging::LogMessage(__FILE__, __LINE__, -(verbose_level)).stream()

#define VLOG(verbose_level) LAZY_STREAM(VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level) && (condition))

#if BUILDFLAG(IS_WIN)
#define VPLOG_STREAM(verbose_level) \
  ::gurl_base::logging::Win32ErrorLogMessage(__FILE__, __LINE__, -(verbose_level), ::gurl_base::logging::GetLastSystemErrorCode()).stream()
#elif BUILDFLAG(IS_POSIX)
#define VPLOG_STREAM(verbose_level) \
  ::gurl_base::logging::ErrnoLogMessage(__FILE__, __LINE__, -(verbose_level), ::gurl_base::logging::GetLastSystemErrorCode()).stream()
#endif

#define VPLOG(verbose_level) LAZY_STREAM(VPLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VPLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VPLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level) && (condition))

// TODO(akalin): Add more VLOG variants, e.g. VPLOG.

#define LOG_ASSERT(condition) LOG_IF(FATAL, !(ABSL_PREDICT_TRUE(condition))) << "Assert failed: " #condition ". "

#if BUILDFLAG(IS_WIN)
#define PLOG_STREAM(severity) \
  COMPACT_LOGGING_EX_##severity(Win32ErrorLogMessage, ::gurl_base::logging::GetLastSystemErrorCode()).stream()
#elif BUILDFLAG(IS_POSIX)
#define PLOG_STREAM(severity) COMPACT_LOGGING_EX_##severity(ErrnoLogMessage, ::gurl_base::logging::GetLastSystemErrorCode()).stream()
#endif

#define PLOG(severity) LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))

#define PLOG_IF(severity, condition) LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

namespace gurl_base {
namespace logging {
extern std::ostream* g_swallow_stream;
}  // namespace logging
}  // namespace gurl_base

// Note that g_swallow_stream is used instead of an arbitrary LOG() stream to
// avoid the creation of an object with a non-trivial destructor (LogMessage).
// On MSVC x86 (checked on 2015 Update 3), this causes a few additional
// pointless instructions to be emitted even at full optimization level, even
// though the : arm of the ternary operator is clearly never executed. Using a
// simpler object to be &'d with Voidify() avoids these extra instructions.
// Using a simpler POD object with a templated operator<< also works to avoid
// these instructions. However, this causes warnings on statically defined
// implementations of operator<<(std::ostream, ...) in some .cc files, because
// they become defined-but-unreferenced functions. A reinterpret_cast of 0 to an
// ostream* also is not suitable, because some compilers warn of undefined
// behavior.
#define EAT_STREAM_PARAMETERS true ? (void)0 : ::gurl_base::logging::LogMessageVoidify() & (*::gurl_base::logging::g_swallow_stream)

// Definitions for DLOG et al.

#if DCHECK_IS_ON()

#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)
#define DPLOG_IF(severity, condition) PLOG_IF(severity, condition)
#define DVLOG_IF(verboselevel, condition) VLOG_IF(verboselevel, condition)
#define DVPLOG_IF(verboselevel, condition) VPLOG_IF(verboselevel, condition)

#else  // !DCHECK_IS_ON()

// If !DCHECK_IS_ON(), we want to avoid emitting any references to |condition|
// (which may reference a variable defined only if DCHECK_IS_ON()).
// Contrast this with DCHECK et al., which has different behavior.

#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DLOG_ASSERT(condition) EAT_STREAM_PARAMETERS
#define DPLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DVLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS
#define DVPLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS

#endif  // DCHECK_IS_ON()

#define DLOG(severity) LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))

#define DPLOG(severity) LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))

#define DVLOG(verboselevel) DVLOG_IF(verboselevel, true)

#define DVPLOG(verboselevel) DVPLOG_IF(verboselevel, true)

// Definitions for DCHECK et al.

constexpr LogSeverity LOGGING_DCHECK = LOGGING_FATAL;

// Redefine the standard assert to use our nice log files
#undef assert
#define assert(x) DLOG_ASSERT(x)

}  // namespace logging
}  // namespace gurl_base

ABSL_DECLARE_FLAG(bool, tick_counts_in_logfile_name);
ABSL_DECLARE_FLAG(bool, logtostderr);
ABSL_DECLARE_FLAG(bool, alsologtostderr);
ABSL_DECLARE_FLAG(bool, colorlogtostderr);
#if BUILDFLAG(IS_POSIX)
ABSL_DECLARE_FLAG(bool, drop_log_memory);
#endif

ABSL_DECLARE_FLAG(int32_t, stderrthreshold);
ABSL_DECLARE_FLAG(int32_t, minloglevel);
ABSL_DECLARE_FLAG(int32_t, logbuflevel);
ABSL_DECLARE_FLAG(int32_t, logbufsecs);

ABSL_DECLARE_FLAG(int32_t, logfile_mode);
ABSL_DECLARE_FLAG(std::string, log_dir);
ABSL_DECLARE_FLAG(std::string, log_link);
ABSL_DECLARE_FLAG(int32_t, max_log_size);
ABSL_DECLARE_FLAG(bool, stop_logging_if_full_disk);
ABSL_DECLARE_FLAG(std::string, log_backtrace_at);

ABSL_DECLARE_FLAG(int32_t, v);
ABSL_DECLARE_FLAG(std::string, vmodule);

ABSL_DECLARE_FLAG(bool, log_process_id);
ABSL_DECLARE_FLAG(bool, log_thread_id);
ABSL_DECLARE_FLAG(bool, log_timestamp);
ABSL_DECLARE_FLAG(bool, log_tickcount);
ABSL_DECLARE_FLAG(std::string, log_prefix);

namespace gurl_base {
namespace logging {

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
#define LOG_TO_SINK(sink, severity) \
  LogMessage(__FILE__, __LINE__, LOGGING_##severity, static_cast<LogSink*>(sink), true).stream()
#define LOG_TO_SINK_BUT_NOT_TO_LOGFILE(sink, severity) \
  LogMessage(__FILE__, __LINE__, LOGGING_##severity, static_cast<LogSink*>(sink), false).stream()

#if defined(_SANITIZE_THREAD)
#define _IFDEF_THREAD_SANITIZER(X) X
#else
#define _IFDEF_THREAD_SANITIZER(X)
#endif

struct CrashReason;

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
  class LogStream : public std::ostream {
   public:
    // 'this' : used in base member initializer
    LogStream(char* buf, int len, uint64_t ctr) : std::ostream(NULL), streambuf_(buf, len), ctr_(ctr), self_(this) {
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

  LogMessage(const char* file, int line, LogSeverity severity, uint64_t ctr, SendMethod send_method);

  // Used for LOG(severity).
  LogMessage(const char* file, int line, LogSeverity severity);

  // Used for CHECK().  Implied severity = LOGGING_FATAL.
  LogMessage(const char* file, int line, const char* condition);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  virtual ~LogMessage();

  std::ostream& stream();

  LogSeverity severity() { return severity_; }

  // Two special constructors that generate reduced amounts of code at
  // LOG call sites for common cases.

  // Used for LOG(INFO): Implied are:
  // severity = INFO, ctr = 0, send_method = &LogMessage::SendToLog.
  //
  // Using this constructor instead of the more complex constructor above
  // saves 19 bytes per call site.
  LogMessage(const char* file, int line);

  // Constructor to log this message to a specified sink (if not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SendToSinkAndLog if
  // also_send_to_log is true, send_method = &LogMessage::SendToSink otherwise.
  LogMessage(const char* file, int line, LogSeverity severity, LogSink* sink, bool also_send_to_log);

  // Constructor where we also give a vector<string> pointer
  // for storing the messages (if the pointer is not NULL).
  // Implied are: ctr = 0, send_method = &LogMessage::SaveOrSendToLog.
  LogMessage(const char* file, int line, LogSeverity severity, std::vector<std::string>* outvec);

  // Constructor where we also give a string pointer for storing the
  // message (if the pointer is not NULL).  Implied are: ctr = 0,
  // send_method = &LogMessage::WriteToStringAndLog.
  LogMessage(const char* file, int line, LogSeverity severity, std::string* message);

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
  [[noreturn]] static void Fail();

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

  void Init(const char* file, int line, LogSeverity severity, void (LogMessage::*send_method)());

  // Used to fill in crash information during LOG(FATAL) failures.
  void RecordCrashReason(CrashReason* reason);

  // Counts of messages sent at each priority:
  static int64_t num_messages_[LOGGING_NUM_SEVERITIES];  // under log_mutex

  // We keep the data in a separate struct so that each instance of
  // LogMessage uses less stack space.
  LogMessageData* allocated_;
  LogMessageData* data_;

  friend class LogDestination;

  const LogSeverity severity_;

  // This is useful since the LogMessage class uses a lot of Win32 calls
  // that will lose the value of GLE and the code that called the log function
  // will have lost the thread error value when the log call returns.
  ScopedClearLastError last_error_;
};

// This class happens to be thread-hostile because all instances share
// a single data buffer, but since it can only be created just before
// the process dies, we don't worry so much.
class LogMessageFatal : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line);
  [[noreturn]] ~LogMessageFatal();
};

// A non-macro interface to the log facility; (useful
// when the logging level is not a compile-time constant).
inline void LogAtLevel(int const severity, std::string const& msg) {
  LogMessage(__FILE__, __LINE__, severity).stream() << msg;
}

// A macro alternative of LogAtLevel. New code may want to use this
// version since there are two advantages: 1. this version outputs the
// file name and the line number where this macro is put like other
// LOG macros, 2. this macro can be used as C++ stream.
#define LOG_AT_LEVEL(severity) LogMessage(__FILE__, __LINE__, severity).stream()

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

#if BUILDFLAG(IS_WIN)
typedef unsigned long SystemErrorCode;
#elif BUILDFLAG(IS_POSIX)
typedef int SystemErrorCode;
#endif

// Alias for ::GetLastError() on Windows and errno on POSIX. Avoids having to
// pull in windows.h just for GetLastError() and DWORD.
SystemErrorCode GetLastSystemErrorCode();
std::string SystemErrorCodeToString(SystemErrorCode error_code);

#if BUILDFLAG(IS_WIN)
// Appends a formatted system message of the GetLastError() type.
class Win32ErrorLogMessage : public LogMessage {
 public:
  Win32ErrorLogMessage(const char* file, int line, LogSeverity severity, SystemErrorCode err);
  Win32ErrorLogMessage(const Win32ErrorLogMessage&) = delete;
  Win32ErrorLogMessage& operator=(const Win32ErrorLogMessage&) = delete;
  // Appends the error message before destructing the encapsulated class.
  ~Win32ErrorLogMessage() override;

 private:
  SystemErrorCode err_;
};
#elif BUILDFLAG(IS_POSIX)
// Appends a formatted system message of the errno type
class ErrnoLogMessage : public LogMessage {
 public:
  ErrnoLogMessage(const char* file, int line, LogSeverity severity, SystemErrorCode err);
  ErrnoLogMessage(const ErrnoLogMessage&) = delete;
  ErrnoLogMessage& operator=(const ErrnoLogMessage&) = delete;
  // Appends the error message before destructing the encapsulated class.
  ~ErrnoLogMessage() override;

 private:
  SystemErrorCode err_;
};
#endif

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
  virtual void Write(bool force_flush, uint64_t tick_counts, const char* message, int message_len) = 0;

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
  [[noreturn]] ~NullStreamFatal() throw() { _exit(1); }
};

// Used by LOG_IS_ON to lazy-evaluate stream arguments.
bool ShouldCreateLogMessage(LogSeverity severity);

// Async signal safe logging mechanism.
void RawLog(int level, const char* message);

#define RAW_LOG(level, message) ::gurl_base::logging::RawLog(::gurl_base::logging::LOGGING_##level, message)

}  // namespace log
}  // namespace gurl_base

#define GURL_DLOG(s) DLOG(s)
#define GURL_LOG(s) LOG(s)

#endif /* POLYFILLS_BASE_LOGGING_H_ */
