// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/logging.hpp"

#define _GNU_SOURCE 1  // needed for O_NOFOLLOW and pread()/pwrite()

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iomanip>
#include <string>
#ifndef OS_WIN
#include <unistd.h>  // For _exit.
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <climits>
#include <csignal>
#if defined(OS_APPLE) || defined(OS_LINUX)
#include <sys/utsname.h>  // For uname.
#endif
#if defined(OS_LINUX)
#include <syscall.h>  // For syscall.
#endif
#if defined(OS_APPLE)
#include <sys/syscall.h>  // For syscall.
#endif
#include <fcntl.h>  // For O_WRONLY, O_CREAT, O_CREAT
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <cerrno>  // for errno
#include <mutex>
#include <sstream>
#include <vector>
#ifdef OS_WIN
#include "core/windows/dirent.h"
#else
#include <dirent.h>  // for automatic removal of old logs
#endif

#include "core/common_posix.hpp"
#include "core/compiler_specific.hpp"
#include "core/debug.hpp"
#include "core/safe_strerror.hpp"
#include "core/string_util.hpp"
#include "core/stringprintf.hpp"
#include "core/utils.hpp"

#ifdef _MSC_VER
// we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#if defined(OS_WIN)
#include <io.h>
#include <windows.h>
typedef HANDLE FileHandle;
// Windows warns on using write().  It prefers _write().
#define safe_write(fd, buf, count) \
  _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2
#elif defined(OS_APPLE)
// Only include TargetConditionals after testing ANDROID as some Android builds
// on the Mac have this header available and it's not needed unless the target
// is really an Apple platform.
#include <TargetConditionals.h>
// In MacOS 10.12 and iOS 10.0 and later ASL (Apple System Log) was deprecated
// in favor of OS_LOG (Unified Logging).
#include <AvailabilityMacros.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
#define USE_ASL
#endif
#else  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#define USE_ASL
#endif
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE

#if defined(USE_ASL)
#include <asl.h>
#else
#include <os/log.h>
#endif
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

// Convert the supplied CFString into the specified encoding, and return it as
// an STL string of the template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template <typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0)
    return StringType();

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring, whole_string, encoding,
                                       0,        // lossByte
                                       false,    // isExternalRepresentation
                                       nullptr,  // buffer
                                       0,        // maxBufLen
                                       &out_size);
  if (converted == 0 || out_size == 0)
    return StringType();

  // out_size is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert out_size by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted =
      CFStringGetBytes(cfstring, whole_string, encoding,
                       0,      // lossByte
                       false,  // isExternalRepresentation
                       reinterpret_cast<UInt8*>(&out_buffer[0]), out_size,
                       nullptr);  // usedBufLen
  if (converted == 0)
    return StringType();

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

#define safe_write(fd, buf, count) write(fd, buf, count)

#elif defined(OS_LINUX) || defined(OS_ANDROID)
#include <time.h>
#define safe_write(fd, buf, count) syscall(SYS_write, fd, buf, count)
#else
#define safe_write(fd, buf, count) write(fd, buf, count)
#endif

#if defined(OS_ANDROID)
#include <android/log.h>
#endif

#if defined(OS_POSIX)
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
#endif

#include <absl/debugging/stacktrace.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/internal/program_name.h>
#include <absl/strings/str_split.h>
#include <absl/synchronization/mutex.h>

#ifndef NDEBUG
#define DEFAULT_ALSOLOGTOSTDERR true
#define DEFAULT_LOGBUFLEVEL -1
#define DEFAULT_VERBOSE_LEVEL 1
#else
#define DEFAULT_ALSOLOGTOSTDERR false
#define DEFAULT_LOGBUFLEVEL 1
#define DEFAULT_VERBOSE_LEVEL 2
#endif

namespace {
// What should be prepended to each message?
bool g_log_process_id = false;
bool g_log_thread_id = false;
bool g_log_timestamp = true;
bool g_log_tickcount = false;
const char* g_log_prefix = nullptr;
}  // namespace

#if defined(OS_APPLE)
// Notes:
// * Xcode's clang did not support `thread_local` until version 8, and
//   even then not for all iOS < 9.0.
// * Xcode 9.3 started disallowing `thread_local` for 32-bit iOS simulator
//   targeting iOS 9.x.
// * Xcode 10 moves the deployment target check for iOS < 9.0 to link time
//   making ABSL_HAVE_FEATURE unreliable there.
//
#if HAVE_CXX11_TLS && \
    !(TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)
#define THREAD_LOCAL_STORAGE thread_local
#endif
#elif defined(HAVE_GCC_TLS)
#define THREAD_LOCAL_STORAGE __thread
#elif defined(HAVE_MSVC_TLS)
#define THREAD_LOCAL_STORAGE __declspec(thread)
#elif defined(HAVE_CXX11_TLS)
#define THREAD_LOCAL_STORAGE thread_lock
#endif

ABSL_FLAG(bool,
          tick_counts_in_logfile_name,
          true,
          "put a tick_counts at the end of the log file name");
ABSL_FLAG(bool,
          logtostderr,
          false,
          "log messages go to stderr instead of logfiles");
ABSL_FLAG(bool,
          alsologtostderr,
          DEFAULT_ALSOLOGTOSTDERR,
          "log messages go to stderr in addition to logfiles");
ABSL_FLAG(bool,
          colorlogtostderr,
          false,
          "color messages logged to stderr (if supported by terminal)");

#if defined(OS_LINUX) || defined(OS_ANDROID)
ABSL_FLAG(bool,
          drop_log_memory,
          true,
          "Drop in-memory buffers of log contents. "
          "Logs can grow very quickly and they are rarely read before they "
          "need to be evicted from memory. Instead, drop them from memory "
          "as soon as they are flushed to disk.");
#endif

// By default, errors (including fatal errors) get logged to stderr as
// well as the file.
//
// The default is ERROR instead of FATAL so that users can see problems
// when they run a program without having to look in another file.
ABSL_FLAG(int32_t,
          stderrthreshold,
          LOGGING_ERROR,
          "log messages at or above this level are copied to stderr in "
          "addition to logfiles.  This flag obsoletes --alsologtostderr.");
ABSL_FLAG(bool,
          log_prefix,
          true,
          "Prepend the log prefix to the start of each log line");
ABSL_FLAG(int32_t,
          minloglevel,
          0,
          "Messages logged at a lower level than this don't "
          "actually get logged anywhere");
ABSL_FLAG(int32_t,
          logbuflevel,
          DEFAULT_LOGBUFLEVEL,
          "Buffer log messages logged at this level or lower"
          " (-1 means don't buffer; 0 means buffer INFO only;"
          " ...)");
ABSL_FLAG(int32_t,
          logbufsecs,
          30,
          "Buffer log messages for at most this many seconds");

// Compute the default value for --log_dir
static const char* DefaultLogDir() {
  const char* env;
  env = getenv("GOOGLE_LOG_DIR");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  env = getenv("TEST_TMPDIR");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  return "";
}

ABSL_FLAG(int32_t, logfile_mode, 0664, "Log file mode/permissions.");

ABSL_FLAG(std::string,
          log_dir,
          DefaultLogDir(),
          "If specified, logfiles are written into this directory instead "
          "of the default logging directory.");
ABSL_FLAG(std::string,
          log_link,
          "",
          "Put additional links to the log "
          "files in this directory");

ABSL_FLAG(int32_t,
          max_log_size,
          1800,
          "approx. maximum log file size (in MB). A value of 0 will "
          "be silently overridden to 1.");

ABSL_FLAG(bool,
          stop_logging_if_full_disk,
          false,
          "Stop attempting to log to disk if the disk is full.");

ABSL_FLAG(std::string,
          log_backtrace_at,
          "",
          "Emit a backtrace when logging at file:linenum.");

// TODO(hamaji): consider windows
#define PATH_SEPARATOR '/'

#ifndef ARRAYSIZE
// There is a better way, but this is good enough for our purpose.
#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

static uint64_t MonotoicTickCount();

uint64_t MonotoicTickCount() {
#if defined(OS_WIN)
  return GetTickCount();
#elif defined(OS_APPLE)
  return mach_absolute_time();
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<int64_t>(ts.tv_sec) * 1000000 +
                            static_cast<int64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

int64_t CycleClock_Now();

int64_t UsecToCycles(int64_t usec);

typedef double WallTime;
WallTime WallTime_Now();

int32_t GetMainThreadPid();
bool PidHasChanged();

#ifdef OS_WIN
// On Windows, process id and thread id are of the same type according to the
// return types of GetProcessId() and GetThreadId() are both DWORD, an unsigned
// 32-bit type.
using pid_t = uint32_t;
#endif
pid_t GetPID();
pid_t GetTID();

const std::string& MyUserName();

// Get the part of filepath after the last path separator.
// (Doesn't modify filepath, contrary to basename() in libgen.h.)
const char* const_basename(const char* filepath);

void DumpStackTraceToString(std::string* stacktrace);

static void NORETURN DumpStackTraceAndExit();

struct CrashReason {
  const char* filename = nullptr;
  int line_number = 0;
  const char* message = nullptr;

  // We'll also store a bit of stack trace context at the time of crash as
  // it may not be available later on.
  void* stack[32];
  int depth = 0;
};

bool SetCrashReason(const CrashReason* r);

static void GetHostName(std::string* hostname) {
#if defined(OS_POSIX)
  struct utsname buf;
  if (0 != uname(&buf)) {
    // ensure null termination on failure
    *buf.nodename = '\0';
  }
  *hostname = buf.nodename;
#elif defined(OS_WIN)
  char buf[MAX_COMPUTERNAME_LENGTH + 1];
  DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
  if (GetComputerNameA(buf, &len)) {
    *hostname = buf;
  } else {
    hostname->clear();
  }
#else
#warning There is no way to retrieve the host name.
  *hostname = "(unknown)";
#endif
}

// Returns true iff terminal supports using colors in output.
static bool TerminalSupportsColor() {
  bool term_supports_color = false;
#ifdef OS_WIN
  // on Windows TERM variable is usually not set, but the console does
  // support colors.
  term_supports_color = true;
#else
  // On non-Windows platforms, we rely on the TERM variable.
  const char* const term = getenv("TERM");
  if (term != nullptr && term[0] != '\0') {
    term_supports_color =
        !strcmp(term, "xterm") || !strcmp(term, "xterm-color") ||
        !strcmp(term, "xterm-256color") || !strcmp(term, "screen-256color") ||
        !strcmp(term, "konsole") || !strcmp(term, "konsole-16color") ||
        !strcmp(term, "konsole-256color") || !strcmp(term, "screen") ||
        !strcmp(term, "linux") || !strcmp(term, "cygwin");
  }
#endif
  return term_supports_color;
}

enum LogColor { COLOR_DEFAULT, COLOR_RED, COLOR_GREEN, COLOR_YELLOW };

static LogColor SeverityToColor(LogSeverity severity) {
  severity = std::max(severity, LOGGING_INFO);
  assert(severity >= 0 && severity < LOGGING_NUM_SEVERITIES);
  LogColor color = COLOR_DEFAULT;
  switch (severity) {
    case LOGGING_INFO:
      color = COLOR_DEFAULT;
      break;
    case LOGGING_WARNING:
      color = COLOR_YELLOW;
      break;
    case LOGGING_ERROR:
    case LOGGING_FATAL:
      color = COLOR_RED;
      break;
    default:
      // should never get here.
      assert(false);
  }
  return color;
}

// For LOGGING_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = static_cast<int>(LOG_ERROR);

namespace {
const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOGGING_NUM_SEVERITIES)
    return log_severity_names[severity];
  if (severity < 0)
    return "VERBOSE";
  return "UNKNOWN";
}

}  // namespace

#ifdef OS_WIN

// Returns the character attribute for the given color.
static WORD GetColorAttribute(LogColor color) {
  switch (color) {
    case COLOR_RED:
      return FOREGROUND_RED;
    case COLOR_GREEN:
      return FOREGROUND_GREEN;
    case COLOR_YELLOW:
      return FOREGROUND_RED | FOREGROUND_GREEN;
    default:
      return 0;
  }
}

#else

// Returns the ANSI color code for the given color.
static const char* GetAnsiColorCode(LogColor color) {
  switch (color) {
    case COLOR_RED:
      return "1";
    case COLOR_GREEN:
      return "2";
    case COLOR_YELLOW:
      return "3";
    case COLOR_DEFAULT:
      return "";
  };
  return nullptr;  // stop warning about return type.
}

#endif  // OS_WIN

// Safely get max_log_size, overriding to 1 if it somehow gets defined as 0
static uint32_t MaxLogSize() {
  return (absl::GetFlag(FLAGS_max_log_size) > 0 &&
                  absl::GetFlag(FLAGS_max_log_size) < 4096
              ? absl::GetFlag(FLAGS_max_log_size)
              : 1);
}

// An arbitrary limit on the length of a single log message.  This
// is so that streaming can be done more efficiently.
const size_t LogMessage::kMaxLogMessageLen = 30000;

struct LogMessage::LogMessageData {
  LogMessageData();

  int preserved_errno_;  // preserved errno
  // Buffer space; contains complete message text.
  char message_text_[LogMessage::kMaxLogMessageLen + 1];
  LogStream stream_;
  int severity_;  // What level is this LogMessage logged at?
  int line_;      // line number where logging call is.
  void (LogMessage::*send_method_)();  // Call this in destructor to send
  union {  // At most one of these is used: union to keep the size low.
    LogSink* sink_;  // nullptr or sink to send message to
    std::vector<std::string>*
        outvec_;            // nullptr or vector to push message onto
    std::string* message_;  // nullptr or string to write message into
  };
  uint64_t tick_counts_;     // Time of creation of LogMessage - montonic
  size_t num_prefix_chars_;  // # of chars of prefix in this message
  size_t num_chars_to_log_;  // # of chars of msg to send to log
  const char* basename_;     // basename of file that called LOG
  const char* fullname_;     // fullname of file that called LOG
  bool has_been_flushed_;    // false => data has not been flushed
  bool first_fatal_;         // true => this was first fatal msg

  LogMessageData(const LogMessageData&) = delete;
  void operator=(const LogMessageData&) = delete;
};

// A mutex that allows only one thread to log at a time, to keep things from
// getting jumbled.  Some other very uncommon logging operations (like
// changing the destination file for log messages of a given severity) also
// lock this mutex.  Please be sure that anybody who might possibly need to
// lock it does so.
static absl::Mutex log_mutex;

// Number of messages sent at each severity.  Under log_mutex.
int64_t LogMessage::num_messages_[LOGGING_NUM_SEVERITIES] = {0, 0, 0, 0};

// Globally disable log writing (if disk is full)
static bool stop_writing = false;

// Has the user called SetExitOnDFatal(true)?
static bool exit_on_dfatal = true;

Logger::~Logger() = default;

// Encapsulates all file-system related state
class LogFileObject : public Logger {
 public:
  LogFileObject(LogSeverity severity, const char* base_filename);
  ~LogFileObject() override;

  void Write(bool force_flush,      // Should we force a flush here?
             uint64_t tick_counts,  // Timestamp for this entry
             const char* message,
             int message_len) override;

  // Configuration options
  void SetBasename(const char* basename);
  void SetExtension(const char* ext);
  void SetSymlinkBasename(const char* symlink_basename);

  // Normal flushing routine
  void Flush() override;

  // It is the actual file length for the system loggers,
  // i.e., INFO, ERROR, etc.
  uint32_t LogSize() override {
    absl::MutexLock l(&lock_);
    return file_length_;
  }

  // Internal flush routine.  Exposed so that FlushLogFilesUnsafe()
  // can avoid grabbing a lock.  Usually Flush() calls it after
  // acquiring lock_.
  void FlushUnlocked();

 private:
  static const uint32_t kRolloverAttemptFrequency = 0x20;

  absl::Mutex lock_;
  bool base_filename_selected_;
  std::string base_filename_;
  std::string symlink_basename_;
  std::string
      filename_extension_;  // option users can specify (eg to add port#)
  FILE* file_;
  LogSeverity severity_;
  uint32_t bytes_since_flush_;
  uint32_t dropped_mem_length_;
  uint32_t file_length_;
  unsigned int rollover_attempt_;
  int64_t next_flush_time_;  // cycle count at which to flush log
  uint64_t start_time_;

  // Actually create a logfile using the value of base_filename_ and the
  // optional argument time_pid_string
  // REQUIRES: lock_ is held
  bool CreateLogfile(const std::string& time_pid_string);
};

// Encapsulate all log cleaner related states
class LogCleaner {
 public:
  LogCleaner() = default;
  ~LogCleaner() = default;

  void Enable(int overdue_days);
  void Disable();
  void Run(bool base_filename_selected,
           const std::string& base_filename,
           const std::string& filename_extension) const;

  inline bool enabled() const { return enabled_; }

 private:
  std::vector<std::string> GetOverdueLogNames(
      std::string log_directory,
      int days,
      const std::string& base_filename,
      const std::string& filename_extension) const;

  bool IsLogFromCurrentProject(const std::string& filepath,
                               const std::string& base_filename,
                               const std::string& filename_extension) const;

  bool IsLogLastModifiedOver(const std::string& filepath, int days) const;

  bool enabled_ = false;
  int overdue_days_ = 7;
  char dir_delim_ =  // filepath delimiter ('/' or '\\')
#ifdef OS_WIN
      '\\';
#else
      '/';
#endif
};

LogCleaner log_cleaner;

class LogDestination {
 public:
  friend class LogMessage;
  friend void ReprintFatalMessage();
  friend Logger* GetLogger(LogSeverity);
  friend void SetLogger(LogSeverity, Logger*);

  // These methods are just forwarded to by their global versions.
  static void SetLogDestination(LogSeverity severity,
                                const char* base_filename);
  static void SetLogSymlink(LogSeverity severity, const char* symlink_basename);
  static void AddLogSink(LogSink* destination);
  static void RemoveLogSink(LogSink* destination);
  static void SetLogFilenameExtension(const char* filename_extension);
  static void SetStderrLogging(LogSeverity min_severity);
  static void LogToStderr();
  // Flush all log files that are at least at the given severity level
  static void FlushLogFiles(int min_severity);
  static void FlushLogFilesUnsafe(int min_severity);

  static const std::string& hostname();
  static const bool& terminal_supports_color() {
    return terminal_supports_color_;
  }

  static void DeleteLogDestinations();

  static bool HasLogDestination(LogSeverity severity);

 private:
  LogDestination(LogSeverity severity, const char* base_filename);
  ~LogDestination();

  // Take a log message of a particular severity and log it to stderr
  // iff it's of a high enough severity to deserve it.
  static void MaybeLogToStderr(LogSeverity severity,
                               const char* message,
                               size_t message_len,
                               size_t prefix_len);
  // Take a log message of a particular severity and log it to a file
  // iff the base filename is not "" (which means "don't log to me")
  static void MaybeLogToLogfile(LogSeverity severity,
                                uint64_t tick_counts,
                                const char* message,
                                size_t len);
  // Take a log message of a particular severity and log it to the file
  // for that severity and also for all files with severity less than
  // this severity.
  static void LogToAllLogfiles(LogSeverity severity,
                               uint64_t tick_counts,
                               const char* message,
                               size_t len);

  // Send logging info to all registered sinks.
  static void LogToSinks(LogSeverity severity,
                         const char* full_filename,
                         const char* base_filename,
                         int line,
                         const char* message,
                         size_t message_len,
                         uint64_t tick_counts);

  // Wait for all registered sinks via WaitTillSent
  // including the optional one in "data".
  static void WaitForSinks(LogMessage::LogMessageData* data);

  static LogDestination* log_destination(LogSeverity severity);

  LogFileObject fileobject_;
  Logger* logger_;  // Either &fileobject_, or wrapper around it

  static LogDestination* log_destinations_[LOGGING_NUM_SEVERITIES];
  static std::string addresses_;
  static std::string hostname_;
  static bool terminal_supports_color_;

  // arbitrary global logging destinations.
  static std::vector<LogSink*>* sinks_;

  // Protects the vector sinks_,
  // but not the LogSink objects its elements reference.
  static absl::Mutex sink_mutex_;

  // Disallow
 public:
  LogDestination(const LogDestination&) = delete;
  LogDestination& operator=(const LogDestination&) = delete;
};

std::string LogDestination::hostname_;

std::vector<LogSink*>* LogDestination::sinks_ = nullptr;
absl::Mutex LogDestination::sink_mutex_;
bool LogDestination::terminal_supports_color_ = TerminalSupportsColor();

/* static */
const std::string& LogDestination::hostname() {
  if (hostname_.empty()) {
    GetHostName(&hostname_);
    if (hostname_.empty()) {
      hostname_ = "(unknown)";
    }
  }
  return hostname_;
}

LogDestination::LogDestination(LogSeverity severity, const char* base_filename)
    : fileobject_(severity, base_filename), logger_(&fileobject_) {}

LogDestination::~LogDestination() {
  if (logger_ && logger_ != &fileobject_) {
    // Delete user-specified logger set via SetLogger().
    delete logger_;
  }
}

inline void LogDestination::FlushLogFilesUnsafe(int /*min_severity*/) {
  // assume we have the log_mutex or we simply don't care
  // about it
  for (LogDestination* log : LogDestination::log_destinations_) {
    if (log != nullptr) {
      // Flush the base fileobject_ logger directly instead of going
      // through any wrappers to reduce chance of deadlock.
      log->fileobject_.FlushUnlocked();
    }
  }
}

inline void LogDestination::FlushLogFiles(int min_severity) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&log_mutex);
  for (int i = min_severity; i < LOGGING_NUM_SEVERITIES; i++) {
    LogDestination* log = log_destination(i);
    if (log != nullptr) {
      log->logger_->Flush();
    }
  }
}

inline void LogDestination::SetLogDestination(LogSeverity severity,
                                              const char* base_filename) {
  severity = std::max(severity, LOGGING_INFO);
  assert(severity >= 0 && severity < LOGGING_NUM_SEVERITIES);
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&log_mutex);
  log_destination(severity)->fileobject_.SetBasename(base_filename);
}

inline void LogDestination::SetLogSymlink(LogSeverity severity,
                                          const char* symlink_basename) {
  severity = std::max(severity, LOGGING_INFO);
  CHECK_LT(severity, LOGGING_NUM_SEVERITIES);
  absl::MutexLock l(&log_mutex);
  log_destination(severity)->fileobject_.SetSymlinkBasename(symlink_basename);
}

inline void LogDestination::AddLogSink(LogSink* destination) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&sink_mutex_);
  if (!sinks_)
    sinks_ = new std::vector<LogSink*>;
  sinks_->push_back(destination);
}

inline void LogDestination::RemoveLogSink(LogSink* destination) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&sink_mutex_);
  // This doesn't keep the sinks in order, but who cares?
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      if ((*sinks_)[i] == destination) {
        (*sinks_)[i] = (*sinks_)[sinks_->size() - 1];
        sinks_->pop_back();
        break;
      }
    }
  }
}

inline void LogDestination::SetLogFilenameExtension(const char* ext) {
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&log_mutex);
  for (int severity = 0; severity < LOGGING_NUM_SEVERITIES; ++severity) {
    log_destination(severity)->fileobject_.SetExtension(ext);
  }
}

inline void LogDestination::SetStderrLogging(LogSeverity min_severity) {
  assert(min_severity >= 0 && min_severity < LOGGING_NUM_SEVERITIES);
  // Prevent any subtle race conditions by wrapping a mutex lock around
  // all this stuff.
  absl::MutexLock l(&log_mutex);
  absl::SetFlag(&FLAGS_stderrthreshold, min_severity);
}

inline void LogDestination::LogToStderr() {
  // *Don't* put this stuff in a mutex lock, since SetStderrLogging &
  // SetLogDestination already do the locking!

  // thus everything is "also" logged to stderr
  SetStderrLogging(LOGGING_INFO);
  for (int i = 0; i < LOGGING_NUM_SEVERITIES; ++i) {
    // "" turns off logging to a logfile
    SetLogDestination(i, "");
  }
}

static void ColoredWriteToStderr(LogSeverity severity,
                                 const char* message,
                                 size_t len) {
  const LogColor color = (LogDestination::terminal_supports_color() &&
                          absl::GetFlag(FLAGS_colorlogtostderr))
                             ? SeverityToColor(severity)
                             : COLOR_DEFAULT;

  // Avoid using cerr from this module since we may get called during
  // exit code, and cerr may be partially or fully destroyed by then.
  if (COLOR_DEFAULT == color) {
    fwrite(message, len, 1, stderr);
    return;
  }
#ifdef OS_WIN
  const HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);

  // Gets the current text color.
  CONSOLE_SCREEN_BUFFER_INFO buffer_info;
  GetConsoleScreenBufferInfo(stderr_handle, &buffer_info);
  const WORD old_color_attrs = buffer_info.wAttributes;

  // We need to flush the stream buffers into the console before each
  // SetConsoleTextAttribute call lest it affect the text that is already
  // printed but has not yet reached the console.
  fflush(stderr);
  SetConsoleTextAttribute(stderr_handle,
                          GetColorAttribute(color) | FOREGROUND_INTENSITY);
  fwrite(message, len, 1, stderr);
  fflush(stderr);
  // Restores the text color.
  SetConsoleTextAttribute(stderr_handle, old_color_attrs);
#else
  fprintf(stderr, "\033[0;3%sm", GetAnsiColorCode(color));
  fwrite(message, len, 1, stderr);
  fprintf(stderr, "\033[m");  // Resets the terminal to default.
#endif  // OS_WIN
}

static void WriteToStderr(const char* message, size_t len) {
  // Avoid using cerr from this module since we may get called during
  // exit code, and cerr may be partially or fully destroyed by then.
  fwrite(message, len, 1, stderr);
}

inline void LogDestination::MaybeLogToStderr(LogSeverity severity,
                                             const char* message,
                                             size_t message_len,
                                             size_t prefix_len) {
  (void)prefix_len;

  // High-severity logs go to stderr by default
  if (severity >= kAlwaysPrintErrorLevel ||
      severity >= absl::GetFlag(FLAGS_stderrthreshold) ||
      absl::GetFlag(FLAGS_alsologtostderr)) {
#ifdef OS_WIN
    // On Windows, also output to the debugger
    ::OutputDebugStringA(message);
#elif defined(OS_APPLE)
    // In LOG_TO_SYSTEM_DEBUG_LOG mode, log messages are always written to
    // stderr. If stderr is /dev/null, also log via ASL (Apple System Log) or
    // its successor OS_LOG. If there's something weird about stderr, assume
    // that log messages are going nowhere and log via ASL/OS_LOG too.
    // Messages logged via ASL/OS_LOG show up in Console.app.
    //
    // Programs started by launchd, as UI applications normally are, have had
    // stderr connected to /dev/null since OS X 10.8. Prior to that, stderr was
    // a pipe to launchd, which logged what it received (see log_redirect_fd in
    // 10.7.5 launchd-392.39/launchd/src/launchd_core_logic.c).
    //
    // Another alternative would be to determine whether stderr is a pipe to
    // launchd and avoid logging via ASL only in that case. See 10.7.5
    // CF-635.21/CFUtilities.c also_do_stderr(). This would result in logging to
    // both stderr and ASL/OS_LOG even in tests, where it's undesirable to log
    // to the system log at all.
    //
    // Note that the ASL client by default discards messages whose levels are
    // below ASL_LEVEL_NOTICE. It's possible to change that with
    // asl_set_filter(), but this is pointless because syslogd normally applies
    // the same filter.
    const bool log_to_system = []() {
      struct stat stderr_stat;
      if (fstat(fileno(stderr), &stderr_stat) == -1) {
        return true;
      }
      if (!S_ISCHR(stderr_stat.st_mode)) {
        return false;
      }

      struct stat dev_null_stat;
      if (stat(_PATH_DEVNULL, &dev_null_stat) == -1) {
        return true;
      }

      return !S_ISCHR(dev_null_stat.st_mode) ||
             stderr_stat.st_rdev == dev_null_stat.st_rdev;
    }();

    if (log_to_system) {
      // Log roughly the same way that CFLog() and NSLog() would. See 10.10.5
      // CF-1153.18/CFUtilities.c __CFLogCString().
      CFBundleRef main_bundle = CFBundleGetMainBundle();
      CFStringRef main_bundle_id_cf =
          main_bundle ? CFBundleGetIdentifier(main_bundle) : nullptr;
      std::string main_bundle_id = main_bundle_id_cf
                                       ? SysCFStringRefToUTF8(main_bundle_id_cf)
                                       : std::string("");
#if defined(USE_ASL)
      // The facility is set to the main bundle ID if available. Otherwise,
      // "com.apple.console" is used.
      const class ASLClient {
       public:
        explicit ASLClient(const std::string& facility)
            : client_(asl_open(nullptr, facility.c_str(), ASL_OPT_NO_DELAY)) {}
        ASLClient(const ASLClient&) = delete;
        ASLClient& operator=(const ASLClient&) = delete;
        ~ASLClient() { asl_close(client_); }

        aslclient get() const { return client_; }

       private:
        aslclient client_;
      } asl_client(main_bundle_id.empty() ? main_bundle_id
                                          : "com.apple.console");

      const class ASLMessage {
       public:
        ASLMessage() : message_(asl_new(ASL_TYPE_MSG)) {}
        ASLMessage(const ASLMessage&) = delete;
        ASLMessage& operator=(const ASLMessage&) = delete;
        ~ASLMessage() { asl_free(message_); }

        aslmsg get() const { return message_; }

       private:
        aslmsg message_;
      } asl_message;

      // By default, messages are only readable by the admin group. Explicitly
      // make them readable by the user generating the messages.
      char euid_string[12];
      snprintf(euid_string, sizeof(euid_string), "%d", geteuid());
      asl_set(asl_message.get(), ASL_KEY_READ_UID, euid_string);

      // Map Chrome log severities to ASL log levels.
      const char* const asl_level_string = [](LogSeverity severity) {
      // ASL_LEVEL_* are ints, but ASL needs equivalent strings. This
      // non-obvious two-step macro trick achieves what's needed.
      // https://gcc.gnu.org/onlinedocs/cpp/Stringification.html
#define ASL_LEVEL_STR(level) ASL_LEVEL_STR_X(level)
#define ASL_LEVEL_STR_X(level) #level
        switch (severity) {
          case LOG_INFO:
            return ASL_LEVEL_STR(ASL_LEVEL_INFO);
          case LOG_WARNING:
            return ASL_LEVEL_STR(ASL_LEVEL_WARNING);
          case LOG_ERROR:
            return ASL_LEVEL_STR(ASL_LEVEL_ERR);
          case LOG_FATAL:
            return ASL_LEVEL_STR(ASL_LEVEL_CRIT);
          default:
            return severity < 0 ? ASL_LEVEL_STR(ASL_LEVEL_DEBUG)
                                : ASL_LEVEL_STR(ASL_LEVEL_NOTICE);
        }
#undef ASL_LEVEL_STR
#undef ASL_LEVEL_STR_X
      }(severity);
      asl_set(asl_message.get(), ASL_KEY_LEVEL, asl_level_string);

      asl_set(asl_message.get(), ASL_KEY_MSG, message);

      asl_send(asl_client.get(), asl_message.get());
#else   // !defined(USE_ASL)
      const class OSLog {
       public:
        explicit OSLog(const char* subsystem)
            : os_log_(subsystem ? os_log_create(subsystem, "yass_logging")
                                : OS_LOG_DEFAULT) {}
        OSLog(const OSLog&) = delete;
        OSLog& operator=(const OSLog&) = delete;
        ~OSLog() {
          if (os_log_ != OS_LOG_DEFAULT) {
            os_release(os_log_);
          }
        }
        os_log_t get() const { return os_log_; }

       private:
        os_log_t os_log_;
      } log(main_bundle_id.empty() ? nullptr : main_bundle_id.c_str());
      const os_log_type_t os_log_type = [](LogSeverity severity) {
        switch (severity) {
          case LOG_INFO:
            return OS_LOG_TYPE_INFO;
          case LOG_WARNING:
            return OS_LOG_TYPE_DEFAULT;
          case LOG_ERROR:
            return OS_LOG_TYPE_ERROR;
          case LOG_FATAL:
            return OS_LOG_TYPE_FAULT;
          default:
            return severity < 0 ? OS_LOG_TYPE_DEBUG : OS_LOG_TYPE_DEFAULT;
        }
      }(severity);
      os_log_with_type(log.get(), os_log_type, "%{public}s", message);
#endif  // defined(USE_ASL)
    }
#elif defined(OS_ANDROID)
    android_LogPriority priority =
        (severity < 0) ? ANDROID_LOG_VERBOSE : ANDROID_LOG_UNKNOWN;
    switch (severity) {
      case LOG_INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case LOG_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
      case LOG_ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case LOG_FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    }
    constexpr char kLogTag[] = "yass";

#if DCHECK_IS_ON()
    // Split the output by new lines to prevent the Android system from
    // truncating the log.
    std::vector<std::string> lines =
        absl::StrSplit(message + prefix_len, "\n", absl::SkipWhitespace());
    for (const auto& line : lines)
      __android_log_write(priority, kLogTag, line.c_str());
#else
    // The Android system may truncate the string if it's too long.
    __android_log_write(priority, kLogTag, message + prefix_len);
#endif  // DCHECK_IS_ON
#endif  // OS_ANDROID

    ColoredWriteToStderr(severity, message, message_len);
  }
}

inline void LogDestination::MaybeLogToLogfile(LogSeverity severity,
                                              uint64_t tick_counts,
                                              const char* message,
                                              size_t len) {
  severity = std::max(severity, LOGGING_INFO);
  const bool should_flush = severity > absl::GetFlag(FLAGS_logbuflevel);
  LogDestination* destination = log_destination(severity);
  destination->logger_->Write(should_flush, tick_counts, message, len);
}

inline void LogDestination::LogToAllLogfiles(LogSeverity severity,
                                             uint64_t tick_counts,
                                             const char* message,
                                             size_t len) {
  severity = std::max(severity, LOGGING_INFO);
  if (absl::GetFlag(FLAGS_logtostderr)) {  // global flag: never log to file
    ColoredWriteToStderr(severity, message, len);
  } else {
    for (int i = severity; i >= 0; --i)
      LogDestination::MaybeLogToLogfile(i, tick_counts, message, len);
  }
}

inline void LogDestination::LogToSinks(LogSeverity severity,
                                       const char* full_filename,
                                       const char* base_filename,
                                       int line,
                                       const char* message,
                                       size_t message_len,
                                       uint64_t tick_counts) {
  absl::ReaderMutexLock l(&sink_mutex_);
  severity = std::max(severity, LOGGING_INFO);
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      (*sinks_)[i]->send(severity, full_filename, base_filename, line, message,
                         message_len, tick_counts);
    }
  }
}

inline void LogDestination::WaitForSinks(LogMessage::LogMessageData* data) {
  absl::ReaderMutexLock l(&sink_mutex_);
  if (sinks_) {
    for (int i = sinks_->size() - 1; i >= 0; i--) {
      (*sinks_)[i]->WaitTillSent();
    }
  }
  const bool send_to_sink =
      (data->send_method_ == &LogMessage::SendToSink) ||
      (data->send_method_ == &LogMessage::SendToSinkAndLog);
  if (send_to_sink && data->sink_ != nullptr) {
    data->sink_->WaitTillSent();
  }
}

LogDestination* LogDestination::log_destinations_[LOGGING_NUM_SEVERITIES];

inline LogDestination* LogDestination::log_destination(LogSeverity severity) {
  severity = std::max(severity, LOGGING_INFO);
  assert(severity >= 0 && severity < LOGGING_NUM_SEVERITIES);
  if (!log_destinations_[severity]) {
    log_destinations_[severity] = new LogDestination(severity, nullptr);
  }
  return log_destinations_[severity];
}

void LogDestination::DeleteLogDestinations() {
  for (auto& log_destination : log_destinations_) {
    delete log_destination;
    log_destination = nullptr;
  }
  absl::MutexLock l(&sink_mutex_);
  delete sinks_;
  sinks_ = nullptr;
}

bool LogDestination::HasLogDestination(LogSeverity severity) {
  severity = std::max(severity, LOGGING_INFO);
  assert(severity >= 0 && severity < LOGGING_NUM_SEVERITIES);
  return log_destinations_[severity];
}

std::string g_application_fingerprint;

void SetApplicationFingerprint(const std::string& fingerprint) {
  g_application_fingerprint = fingerprint;
}

LogFileObject::LogFileObject(LogSeverity severity, const char* base_filename)
    : base_filename_selected_(base_filename != nullptr),
      base_filename_((base_filename != nullptr) ? base_filename : ""),
      symlink_basename_(absl::flags_internal::ShortProgramInvocationName()),
      file_(nullptr),
      severity_(severity),
      bytes_since_flush_(0),
      dropped_mem_length_(0),
      file_length_(0),
      rollover_attempt_(kRolloverAttemptFrequency - 1),
      next_flush_time_(0),
      start_time_(MonotoicTickCount()) {
  assert(severity < LOGGING_NUM_SEVERITIES);
}

LogFileObject::~LogFileObject() {
  absl::MutexLock l(&lock_);
  if (file_ != nullptr) {
    fclose(file_);
    file_ = nullptr;
  }
}

void LogFileObject::SetBasename(const char* basename) {
  absl::MutexLock l(&lock_);
  base_filename_selected_ = true;
  if (base_filename_ != basename) {
    // Get rid of old log file since we are changing names
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
      rollover_attempt_ = kRolloverAttemptFrequency - 1;
    }
    base_filename_ = basename;
  }
}

void LogFileObject::SetExtension(const char* ext) {
  absl::MutexLock l(&lock_);
  if (filename_extension_ != ext) {
    // Get rid of old log file since we are changing names
    if (file_ != nullptr) {
      fclose(file_);
      file_ = nullptr;
      rollover_attempt_ = kRolloverAttemptFrequency - 1;
    }
    filename_extension_ = ext;
  }
}

void LogFileObject::SetSymlinkBasename(const char* symlink_basename) {
  absl::MutexLock l(&lock_);
  symlink_basename_ = symlink_basename;
}

void LogFileObject::Flush() {
  absl::MutexLock l(&lock_);
  FlushUnlocked();
}

void LogFileObject::FlushUnlocked() {
  if (file_ != nullptr) {
    fflush(file_);
    bytes_since_flush_ = 0;
  }
  // Figure out when we are due for another flush.
  const int64_t next = (absl::GetFlag(FLAGS_logbufsecs) *
                        static_cast<int64_t>(1000000));  // in usec
  next_flush_time_ = CycleClock_Now() + UsecToCycles(next);
}

bool LogFileObject::CreateLogfile(const std::string& time_pid_string) {
  std::string string_filename = base_filename_;
  if (absl::GetFlag(FLAGS_tick_counts_in_logfile_name)) {
    string_filename += time_pid_string;
  }
  string_filename += filename_extension_;
  const char* filename = string_filename.c_str();
  // only write to files, create if non-existant.
  int flags = O_WRONLY | O_CREAT;
  if (absl::GetFlag(FLAGS_tick_counts_in_logfile_name)) {
    // demand that the file is unique for our tick_counts (fail if it exists).
    flags = flags | O_EXCL;
  }
  int fd = open(filename, flags, absl::GetFlag(FLAGS_logfile_mode));
  if (fd == -1)
    return false;
#ifdef HAVE_FLOCK
  // Mark the file close-on-exec. We don't really care if this fails
  fcntl(fd, F_SETFD, FD_CLOEXEC);

  // Mark the file as exclusive write access to avoid two clients logging to the
  // same file. This applies particularly when
  // !FLAGS_tick_counts_in_logfile_name (otherwise open would fail because the
  // O_EXCL flag on similar filename). locks are released on unlock or close()
  // automatically, only after log is released. This will work after a fork as
  // it is not inherited (not stored in the fd). Lock will not be lost because
  // the file is opened with exclusive lock (write) and we will never read from
  // it inside the process.
  // TODO windows implementation of this (as flock is not available on mingw).
  static struct flock w_lock;

  w_lock.l_type = F_WRLCK;
  w_lock.l_start = 0;
  w_lock.l_whence = SEEK_SET;
  w_lock.l_len = 0;

  int wlock_ret = fcntl(fd, F_SETLK, &w_lock);
  if (wlock_ret == -1) {
    close(fd);  // as we are failing already, do not check errors here
    return false;
  }
#endif

  // fdopen in append mode so if the file exists it will fseek to the end
  file_ = fdopen(fd, "a");  // Make a FILE*.
  if (file_ == nullptr) {   // Man, we're screwed!
    close(fd);
    if (absl::GetFlag(FLAGS_tick_counts_in_logfile_name)) {
      unlink(filename);  // Erase the half-baked evidence: an unusable log file,
                         // only if we just created it.
    }
    return false;
  }
#ifdef OS_WIN
  // https://github.com/golang/go/issues/27638 - make sure we seek to the end to
  // append empirically replicated with wine over mingw build
  if (!absl::GetFlag(FLAGS_tick_counts_in_logfile_name)) {
    if (fseek(file_, 0, SEEK_END) != 0) {
      return false;
    }
  }
#endif
  // We try to create a symlink called <program_name>.<severity>,
  // which is easier to use.  (Every time we create a new logfile,
  // we destroy the old symlink and create a new one, so it always
  // points to the latest logfile.)  If it fails, we're sad but it's
  // no error.
  if (!symlink_basename_.empty()) {
    // take directory from filename
    const char* slash = strrchr(filename, PATH_SEPARATOR);
    const std::string linkname =
        symlink_basename_ + '.' + log_severity_name(severity_);
    std::string linkpath;
    // get dirname
    if (slash)
      linkpath = std::string(filename, slash - filename + 1);
    linkpath += linkname;
    unlink(linkpath.c_str());  // delete old one if it exists

#if defined(OS_WIN)
    // TODO(hamaji): Create lnk file on Windows?
#else
    // We must have unistd.h.
    // Make the symlink be relative (in the same dir) so that if the
    // entire log directory gets relocated the link is still valid.
    const char* linkdest = slash ? (slash + 1) : filename;
    if (symlink(linkdest, linkpath.c_str()) != 0) {
      // silently ignore failures
    }

    // Make an additional link to the log file in a place specified by
    // FLAGS_log_link, if indicated
    if (!absl::GetFlag(FLAGS_log_link).empty()) {
      linkpath = absl::GetFlag(FLAGS_log_link) + "/" + linkname;
      unlink(linkpath.c_str());  // delete old one if it exists
      if (symlink(filename, linkpath.c_str()) != 0) {
        // silently ignore failures
      }
    }
#endif
  }

  return true;  // Everything worked
}

void LogFileObject::Write(bool force_flush,
                          uint64_t /*tick_counts*/,
                          const char* message,
                          int message_len) {
  absl::MutexLock l(&lock_);

  // We don't log if the base_name_ is "" (which means "don't write")
  if (base_filename_selected_ && base_filename_.empty()) {
    return;
  }

  if (static_cast<unsigned int>(file_length_ >> 20) >= MaxLogSize() ||
      PidHasChanged()) {
    if (file_ != nullptr)
      fclose(file_);
    file_ = nullptr;
    file_length_ = bytes_since_flush_ = dropped_mem_length_ = 0;
    rollover_attempt_ = kRolloverAttemptFrequency - 1;
  }

  // If there's no destination file, make one before outputting
  if (file_ == nullptr) {
    // Try to rollover the log file every 32 log messages.  The only time
    // this could matter would be when we have trouble creating the log
    // file.  If that happens, we'll lose lots of log messages, of course!
    if (++rollover_attempt_ != kRolloverAttemptFrequency)
      return;
    rollover_attempt_ = 0;

    time_t timestamp = WallTime_Now();
    struct ::tm tm_time;
#ifdef OS_WIN
    localtime_s(&tm_time, &timestamp);
#else
    localtime_r(&timestamp, &tm_time);
#endif

    // The logfile's filename will have the date/time & pid in it
    std::ostringstream time_pid_stream;
    time_pid_stream.fill('0');
    time_pid_stream << 1900 + tm_time.tm_year << std::setw(2)
                    << 1 + tm_time.tm_mon << std::setw(2) << tm_time.tm_mday
                    << '-' << std::setw(2) << tm_time.tm_hour << std::setw(2)
                    << tm_time.tm_min << std::setw(2) << tm_time.tm_sec << '.'
                    << GetMainThreadPid();
    const std::string& time_pid_string = time_pid_stream.str();

    if (base_filename_selected_) {
      if (!CreateLogfile(time_pid_string)) {
        perror("Could not create log file");
        fprintf(stderr, "COULD NOT CREATE LOGFILE '%s'!\n",
                time_pid_string.c_str());
        return;
      }
    } else {
      // If no base filename for logs of this severity has been set, use a
      // default base filename of
      // "<program name>.<hostname>.<user name>.log.<severity level>.".  So
      // logfiles will have names like
      // webserver.examplehost.root.log.INFO.19990817-150000.4354, where
      // 19990817 is a date (1999 August 17), 150000 is a time (15:00:00),
      // and 4354 is the pid of the logging process.  The date & time reflect
      // when the file was created for output.
      //
      // Where does the file get put?  Successively try the directories
      // "/tmp", and "."
      std::string stripped_filename(
          absl::flags_internal::ShortProgramInvocationName());
      std::string hostname;
      GetHostName(&hostname);

      std::string uidname = MyUserName();
      // We should not call CHECK() here because this function can be
      // called after holding on to log_mutex. We don't want to
      // attempt to hold on to the same mutex, and get into a
      // deadlock. Simply use a name like invalid-user.
      if (uidname.empty())
        uidname = "invalid-user";

      stripped_filename = stripped_filename + '.' + hostname + '.' + uidname +
                          ".log." + log_severity_name(severity_) + '.';
      // We're going to (potentially) try to put logs in several different dirs
      const std::vector<std::string>& log_dirs = GetLoggingDirectories();

      // Go through the list of dirs, and try to create the log file in each
      // until we succeed or run out of options
      bool success = false;
      for (const std::string& log_dir : log_dirs) {
        base_filename_ = log_dir + "/" + stripped_filename;
        if (CreateLogfile(time_pid_string)) {
          success = true;
          break;
        }
      }
      // If we never succeeded, we have to give up
      if (success == false) {
        perror("Could not create logging file");
        fprintf(stderr, "COULD NOT CREATE A LOGGINGFILE %s!",
                time_pid_string.c_str());
        return;
      }
    }

    // Write a header message into the log file
    std::ostringstream file_header_stream;
    file_header_stream.fill('0');
    file_header_stream << "Log file created at: " << 1900 + tm_time.tm_year
                       << '/' << std::setw(2) << 1 + tm_time.tm_mon << '/'
                       << std::setw(2) << tm_time.tm_mday << ' ' << std::setw(2)
                       << tm_time.tm_hour << ':' << std::setw(2)
                       << tm_time.tm_min << ':' << std::setw(2)
                       << tm_time.tm_sec << "\n"
                       << "Running on machine: " << LogDestination::hostname()
                       << '\n';

    if (!g_application_fingerprint.empty()) {
      file_header_stream << "Application fingerprint: "
                         << g_application_fingerprint << '\n';
    }

    file_header_stream << "Running duration (monotonic time): "
                       << MonotoicTickCount() - start_time_ << '\n'
                       << "Log line format: ";

    file_header_stream << '[';
    if (g_log_prefix)
      file_header_stream << g_log_prefix << ':';
    if (g_log_process_id)
      file_header_stream << "pid" << ':';
    if (g_log_thread_id)
      file_header_stream << "tid" << ':';
    if (g_log_timestamp)
      file_header_stream << "MMDD/HHMMSS.usec" << ':';
    if (g_log_tickcount)
      file_header_stream << "tickcount" << ':';
    file_header_stream << "L:file(line)] msg\n";

    const std::string& file_header_string = file_header_stream.str();

    const int header_len = file_header_string.size();
    fwrite(file_header_string.data(), 1, header_len, file_);
    file_length_ += header_len;
    bytes_since_flush_ += header_len;
  }

  // Write to LOG file
  if (!stop_writing) {
    // fwrite() doesn't return an error when the disk is full, for
    // messages that are less than 4096 bytes. When the disk is full,
    // it returns the message length for messages that are less than
    // 4096 bytes. fwrite() returns 4096 for message lengths that are
    // greater than 4096, thereby indicating an error.
    errno = 0;
    fwrite(message, 1, message_len, file_);
    if (absl::GetFlag(FLAGS_stop_logging_if_full_disk) &&
        errno == ENOSPC) {  // disk full, stop writing to disk
      stop_writing = true;  // until the disk is
      return;
    } else {
      file_length_ += message_len;
      bytes_since_flush_ += message_len;
    }
  } else {
    if (CycleClock_Now() >= next_flush_time_)
      stop_writing = false;  // check to see if disk has free space.
    return;                  // no need to flush
  }

  // See important msgs *now*.  Also, flush logs at least every 10^6 chars,
  // or every "FLAGS_logbufsecs" seconds.
  if (force_flush || (bytes_since_flush_ >= 1000000) ||
      (CycleClock_Now() >= next_flush_time_)) {
    FlushUnlocked();
#if defined(OS_LINUX) || defined(OS_ANDROID)
    // Only consider files >= 3MiB
    if (absl::GetFlag(FLAGS_drop_log_memory) && file_length_ >= (3 << 20)) {
      // Don't evict the most recent 1-2MiB so as not to impact a tailer
      // of the log file and to avoid page rounding issue on linux < 4.7
      uint32_t total_drop_length =
          (file_length_ & ~((1 << 20) - 1)) - (1 << 20);
      uint32_t this_drop_length = total_drop_length - dropped_mem_length_;
      if (this_drop_length >= (2 << 20)) {
        // Only advise when >= 2MiB to drop
#if defined(OS_ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
        // 'posix_fadvise' introduced in API 21:
        // * https://android.googlesource.com/platform/bionic/+/6880f936173081297be0dc12f687d341b86a4cfa/libc/libc.map.txt#732
#else
        posix_fadvise(fileno(file_), dropped_mem_length_, this_drop_length,
                      POSIX_FADV_DONTNEED);
#endif
        dropped_mem_length_ = total_drop_length;
      }
    }
#endif

    // Perform clean up for old logs
    if (log_cleaner.enabled()) {
      if (base_filename_selected_ && base_filename_.empty()) {
        return;
      }
      log_cleaner.Run(base_filename_selected_, base_filename_,
                      filename_extension_);
    }
  }
}

void LogCleaner::Enable(int overdue_days) {
  // Setting overdue_days to 0 day should not be allowed!
  // Since all logs will be deleted immediately, which will cause troubles.
  assert(overdue_days > 0);

  enabled_ = true;
  overdue_days_ = overdue_days;
}

void LogCleaner::Disable() {
  enabled_ = false;
}

void LogCleaner::Run(bool base_filename_selected,
                     const std::string& base_filename,
                     const std::string& filename_extension) const {
  assert(enabled_ && overdue_days_ > 0);

  std::vector<std::string> dirs;

  if (base_filename_selected) {
    std::string dir =
        base_filename.substr(0, base_filename.find_last_of(dir_delim_) + 1);
    dirs.push_back(dir);
  } else {
    dirs = GetLoggingDirectories();
  }

  for (const std::string& dir : dirs) {
    std::vector<std::string> logs = GetOverdueLogNames(
        dir, overdue_days_, base_filename, filename_extension);
    for (const std::string& log : logs) {
      static_cast<void>(unlink(log.c_str()));
    }
  }
}

std::vector<std::string> LogCleaner::GetOverdueLogNames(
    std::string log_directory,
    int days,
    const std::string& base_filename,
    const std::string& filename_extension) const {
  // The names of overdue logs.
  std::vector<std::string> overdue_log_names;

  // Try to get all files within log_directory.
  DIR* dir;
  struct dirent* ent;

  // If log_directory doesn't end with a slash, append a slash to it.
  if (log_directory.at(log_directory.size() - 1) != dir_delim_) {
    log_directory += dir_delim_;
  }

  if ((dir = opendir(log_directory.c_str())) != nullptr) {
    while ((ent = readdir(dir)) != nullptr) {
      if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
        continue;
      }
      std::string filepath = log_directory + ent->d_name;
      if (IsLogFromCurrentProject(filepath, base_filename,
                                  filename_extension) &&
          IsLogLastModifiedOver(filepath, days)) {
        overdue_log_names.push_back(filepath);
      }
    }
    closedir(dir);
  }

  return overdue_log_names;
}

bool LogCleaner::IsLogFromCurrentProject(
    const std::string& filepath,
    const std::string& base_filename,
    const std::string& filename_extension) const {
  // We should remove duplicated delimiters from `base_filename`, e.g.,
  // before: "/tmp//<base_filename>.<create_time>.<pid>"
  // after:  "/tmp/<base_filename>.<create_time>.<pid>"
  std::string cleaned_base_filename;

  size_t real_filepath_size = filepath.size();
  for (const char& c : base_filename) {
    if (cleaned_base_filename.empty()) {
      cleaned_base_filename += c;
    } else if (c != dir_delim_ || c != cleaned_base_filename.at(
                                           cleaned_base_filename.size() - 1)) {
      cleaned_base_filename += c;
    }
  }

  // Return early if the filename doesn't start with `cleaned_base_filename`.
  if (filepath.find(cleaned_base_filename) != 0) {
    return false;
  }

  // Check if in the string `filename_extension` is right next to
  // `cleaned_base_filename` in `filepath` if the user
  // has set a custom filename extension.
  if (!filename_extension.empty()) {
    if (cleaned_base_filename.size() >= real_filepath_size) {
      return false;
    }
    // for origin version, `filename_extension` is middle of the `filepath`.
    std::string ext = filepath.substr(cleaned_base_filename.size(),
                                      filename_extension.size());
    if (ext == filename_extension) {
      cleaned_base_filename += filename_extension;
    } else {
      // for new version, `filename_extension` is right of the `filepath`.
      if (filename_extension.size() >= real_filepath_size) {
        return false;
      }
      real_filepath_size = filepath.size() - filename_extension.size();
      if (filepath.substr(real_filepath_size) != filename_extension) {
        return false;
      }
    }
  }

  // The characters after `cleaned_base_filename` should match the format:
  // YYYYMMDD-HHMMSS.pid
  for (size_t i = cleaned_base_filename.size(); i < real_filepath_size; i++) {
    const char& c = filepath[i];

    if (i <= cleaned_base_filename.size() + 7) {  // 0 ~ 7 : YYYYMMDD
      if (c < '0' || c > '9') {
        return false;
      }

    } else if (i == cleaned_base_filename.size() + 8) {  // 8: -
      if (c != '-') {
        return false;
      }

    } else if (i <= cleaned_base_filename.size() + 14) {  // 9 ~ 14: HHMMSS
      if (c < '0' || c > '9') {
        return false;
      }

    } else if (i == cleaned_base_filename.size() + 15) {  // 15: .
      if (c != '.') {
        return false;
      }

    } else if (i >= cleaned_base_filename.size() + 16) {  // 16+: pid
      if (c < '0' || c > '9') {
        return false;
      }
    }
  }

  return true;
}

bool LogCleaner::IsLogLastModifiedOver(const std::string& filepath,
                                       int days) const {
  // Try to get the last modified time of this file.
  struct stat file_stat;

  if (stat(filepath.c_str(), &file_stat) == 0) {
    // A day is 86400 seconds, so 7 days is 86400 * 7 = 604800 seconds.
    time_t last_modified_time = file_stat.st_mtime;
    time_t current_time = time(nullptr);
    return difftime(current_time, last_modified_time) > days * 86400;
  }

  // If failed to get file stat, don't return true!
  return false;
}

// Static log data space to avoid alloc failures in a LOG(FATAL)
//
// Since multiple threads may call LOG(FATAL), and we want to preserve
// the data from the first call, we allocate two sets of space.  One
// for exclusive use by the first thread, and one for shared use by
// all other threads.
static absl::Mutex fatal_msg_lock;
static CrashReason crash_reason;
static bool fatal_msg_exclusive = true;
static LogMessage::LogMessageData fatal_msg_data_exclusive;
static LogMessage::LogMessageData fatal_msg_data_shared;

#ifdef THREAD_LOCAL_STORAGE
// Static thread-local log data space to use, because typically at most one
// LogMessageData object exists (in this case log makes zero heap memory
// allocations).
static THREAD_LOCAL_STORAGE bool thread_data_available = true;

#ifdef HAVE_ALIGNED_STORAGE
static THREAD_LOCAL_STORAGE std::aligned_storage<
    sizeof(LogMessage::LogMessageData),
    alignof(LogMessage::LogMessageData)>::type thread_msg_data;
#else
static THREAD_LOCAL_STORAGE char
    thread_msg_data[sizeof(void*) + sizeof(LogMessage::LogMessageData)];
#endif  // HAVE_ALIGNED_STORAGE
#endif  // defined(THREAD_LOCAL_STORAGE)

LogMessage::LogMessageData::LogMessageData()
    : stream_(message_text_, LogMessage::kMaxLogMessageLen, 0) {}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       uint64_t ctr,
                       void (LogMessage::*send_method)())
    : allocated_(nullptr), severity_(severity) {
  Init(file, line, severity, send_method);
  data_->stream_.set_ctr(ctr);
}

LogMessage::LogMessage(const char* file, int line)
    : allocated_(nullptr), severity_(LOGGING_INFO) {
  Init(file, line, LOGGING_INFO, &LogMessage::SendToLog);
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : allocated_(nullptr), severity_(severity) {
  Init(file, line, severity, &LogMessage::SendToLog);
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       LogSink* sink,
                       bool also_send_to_log)
    : allocated_(nullptr), severity_(severity) {
  Init(file, line, severity,
       also_send_to_log ? &LogMessage::SendToSinkAndLog
                        : &LogMessage::SendToSink);
  data_->sink_ = sink;  // override Init()'s setting to nullptr
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       std::vector<std::string>* outvec)
    : allocated_(nullptr), severity_(severity) {
  Init(file, line, severity, &LogMessage::SaveOrSendToLog);
  data_->outvec_ = outvec;  // override Init()'s setting to nullptr
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       std::string* message)
    : allocated_(nullptr), severity_(severity) {
  Init(file, line, severity, &LogMessage::WriteToStringAndLog);
  data_->message_ = message;  // override Init()'s setting to nullptr
}

void LogMessage::Init(const char* file,
                      int line,
                      LogSeverity severity,
                      void (LogMessage::*send_method)()) {
  allocated_ = nullptr;

  if (severity != LOGGING_FATAL || !exit_on_dfatal) {
#ifdef THREAD_LOCAL_STORAGE
    // No need for locking, because this is thread local.
    if (thread_data_available) {
      thread_data_available = false;
#ifdef HAVE_ALIGNED_STORAGE
      data_ = new (&thread_msg_data) LogMessageData;
#else
      const uintptr_t kAlign = sizeof(void*) - 1;

      char* align_ptr = reinterpret_cast<char*>(
          reinterpret_cast<uintptr_t>(thread_msg_data + kAlign) & ~kAlign);
      data_ = new (align_ptr) LogMessageData;
      assert(reinterpret_cast<uintptr_t>(align_ptr) % sizeof(void*) == 0);
#endif
    } else {
      allocated_ = new LogMessageData();
      data_ = allocated_;
    }
#else   // !defined(THREAD_LOCAL_STORAGE)
    allocated_ = new LogMessageData();
    data_ = allocated_;
#endif  // defined(THREAD_LOCAL_STORAGE)
    data_->first_fatal_ = false;
  } else {
    absl::MutexLock l(&fatal_msg_lock);
    if (fatal_msg_exclusive) {
      fatal_msg_exclusive = false;
      data_ = &fatal_msg_data_exclusive;
      data_->first_fatal_ = true;
    } else {
      data_ = &fatal_msg_data_shared;
      data_->first_fatal_ = false;
    }
  }

  stream().fill('0');
  data_->preserved_errno_ = errno;
  data_->severity_ = severity;
  data_->line_ = line;
  data_->send_method_ = send_method;
  data_->outvec_ = nullptr;
  data_->sink_ = nullptr;
  data_->tick_counts_ = MonotoicTickCount();

  data_->num_chars_to_log_ = 0;
  data_->basename_ = const_basename(file);
  data_->fullname_ = file;
  data_->has_been_flushed_ = false;

  // If specified, prepend a prefix to each line.  For example:
  //    I20201018 160715 f5d4fbb0 logging.cc:1153]
  //    (log level, GMT year, month, date, time, thread_id, file basename, line)
  // We exclude the thread_id for the default thread.
  if (absl::GetFlag(FLAGS_log_prefix) && (line != kNoLogPrefix)) {
    // TODO(darin): It might be nice if the columns were fixed width.
    stream() << '[';
    if (g_log_prefix)
      stream() << g_log_prefix << ':';
    if (g_log_process_id)
      stream() << GetMainThreadPid() << ':';
    if (g_log_thread_id)
      stream() << GetTID() << ':';
    if (g_log_timestamp) {
#if defined(OS_WIN)
      SYSTEMTIME local_time;
      GetLocalTime(&local_time);
      stream() << std::setfill('0') << std::setw(2) << local_time.wMonth
               << std::setw(2) << local_time.wDay << '/' << std::setw(2)
               << local_time.wHour << std::setw(2) << local_time.wMinute
               << std::setw(2) << local_time.wSecond << '.' << std::setw(3)
               << local_time.wMilliseconds << ':';
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      timeval tv;
      gettimeofday(&tv, nullptr);
      time_t t = tv.tv_sec;
      struct tm local_time;
      localtime_r(&t, &local_time);
      struct tm* tm_time = &local_time;
      stream() << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
               << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
               << tm_time->tm_hour << std::setw(2) << tm_time->tm_min
               << std::setw(2) << tm_time->tm_sec << '.' << std::setw(6)
               << tv.tv_usec << ':';
#else
#error Unsupported platform
#endif
    }
    if (g_log_tickcount)
      stream() << data_->tick_counts_ << ':';
    if (severity_ >= 0) {
      stream() << log_severity_name(severity_);
    } else {
      stream() << "VERBOSE" << -severity_;
    }
    stream() << ":" << data_->basename_ << "(" << data_->line_ << ")] ";
  }
  data_->num_prefix_chars_ = data_->stream_.pcount();

  if (!absl::GetFlag(FLAGS_log_backtrace_at).empty()) {
    char fileline[128];
    snprintf(fileline, sizeof(fileline), "%s:%d", data_->basename_, line);
    if (!strcmp(absl::GetFlag(FLAGS_log_backtrace_at).c_str(), fileline)) {
      std::string stacktrace;
      DumpStackTraceToString(&stacktrace);
      stream() << " (stacktrace:\n" << stacktrace << ") ";
    }
  }
}

LogMessage::~LogMessage() {
  Flush();
#ifdef THREAD_LOCAL_STORAGE
  if (data_ == static_cast<void*>(&thread_msg_data)) {
    data_->~LogMessageData();
    thread_data_available = true;
  } else {
    delete allocated_;
  }
#else   // !defined(THREAD_LOCAL_STORAGE)
  delete allocated_;
#endif  // defined(THREAD_LOCAL_STORAGE)
}

int LogMessage::preserved_errno() const {
  return data_->preserved_errno_;
}

std::ostream& LogMessage::stream() {
  return data_->stream_;
}

namespace {
#if defined(OS_ANDROID)
int AndroidLogLevel(const int severity) {
  switch (severity) {
    case 3:
      return ANDROID_LOG_FATAL;
    case 2:
      return ANDROID_LOG_ERROR;
    case 1:
      return ANDROID_LOG_WARN;
    default:
      return ANDROID_LOG_INFO;
  }
}
#endif  // defined(OS_ANDROID)
}  // namespace

// Flush buffered message, called by the destructor, or any other function
// that needs to synchronize the log.
void LogMessage::Flush() {
  if (data_->has_been_flushed_ ||
      data_->severity_ < absl::GetFlag(FLAGS_minloglevel))
    return;

  data_->num_chars_to_log_ = data_->stream_.pcount();

  // Do we need to add a \n to the end of this message?
  bool append_newline =
      (data_->message_text_[data_->num_chars_to_log_ - 1] != '\n');
  char original_final_char = '\0';

  // If we do need to add a \n, we'll do it by violating the memory of the
  // ostrstream buffer.  This is quick, and we'll make sure to undo our
  // modification before anything else is done with the ostrstream.  It
  // would be preferable not to do things this way, but it seems to be
  // the best way to deal with this.
  if (append_newline) {
    original_final_char = data_->message_text_[data_->num_chars_to_log_];
    data_->message_text_[data_->num_chars_to_log_++] = '\n';
  }
  data_->message_text_[data_->num_chars_to_log_] = '\0';

  // Prevent any subtle race conditions by wrapping a mutex lock around
  // the actual logging action per se.
  {
    absl::MutexLock l(&log_mutex);
    (this->*(data_->send_method_))();
    ++num_messages_[std::max(data_->severity_, LOGGING_INFO)];
  }
  LogDestination::WaitForSinks(data_);

#if defined(OS_ANDROID)
  const int level = AndroidLogLevel(data_->severity_);
  const std::string text = std::string(data_->message_text_);
  __android_log_write(level, "native",
                      text.substr(0, data_->num_chars_to_log_).c_str());
#endif  // defined(OS_ANDROID)

  if (append_newline) {
    // Fix the ostrstream back how it was before we screwed with it.
    // It's 99.44% certain that we don't need to worry about doing this.
    data_->message_text_[data_->num_chars_to_log_ - 1] = original_final_char;
  }

  // If errno was already set before we enter the logging call, we'll
  // set it back to that value when we return from the logging call.
  // It happens often that we log an error message after a syscall
  // failure, which can potentially set the errno to some other
  // values.  We would like to preserve the original errno.
  if (data_->preserved_errno_ != 0) {
    errno = data_->preserved_errno_;
  }

  // Note that this message is now safely logged.  If we're asked to flush
  // again, as a result of destruction, say, we'll do nothing on future calls.
  data_->has_been_flushed_ = true;
}

// Copy of first FATAL log message so that we can print it out again
// after all the stack traces.  To preserve legacy behavior, we don't
// use fatal_msg_data_exclusive.
static uint64_t fatal_time;
static char fatal_message[256];

void ReprintFatalMessage() {
  if (fatal_message[0]) {
    const int n = strlen(fatal_message);
    if (!absl::GetFlag(FLAGS_logtostderr)) {
      // Also write to stderr (don't color to avoid terminal checks)
      WriteToStderr(fatal_message, n);
    }
    LogDestination::LogToAllLogfiles(LOGGING_ERROR, fatal_time, fatal_message,
                                     n);
  }
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SendToLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  log_mutex.AssertHeld();

  assert(data_->num_chars_to_log_ > 0 &&
         data_->message_text_[data_->num_chars_to_log_ - 1] == '\n');

  // Messages of a given severity get logged to lower severity logs, too

  // global flag: never log to file if set.  Also -- don't log to a
  // file if we haven't parsed the command line flags to get the
  // program name.
  if (absl::GetFlag(FLAGS_logtostderr)) {
    ColoredWriteToStderr(data_->severity_, data_->message_text_,
                         data_->num_chars_to_log_);

    // this could be protected by a flag if necessary.
    LogDestination::LogToSinks(
        data_->severity_, data_->fullname_, data_->basename_, data_->line_,
        data_->message_text_ + data_->num_prefix_chars_,
        (data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1),
        data_->tick_counts_);
  } else {
    // log this message to all log files of severity <= severity_
    LogDestination::LogToAllLogfiles(data_->severity_, data_->tick_counts_,
                                     data_->message_text_,
                                     data_->num_chars_to_log_);

    LogDestination::MaybeLogToStderr(data_->severity_, data_->message_text_,
                                     data_->num_chars_to_log_,
                                     data_->num_prefix_chars_);
    LogDestination::LogToSinks(
        data_->severity_, data_->fullname_, data_->basename_, data_->line_,
        data_->message_text_ + data_->num_prefix_chars_,
        (data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1),
        data_->tick_counts_);
    // NOTE: -1 removes trailing \n
  }

  // If we log a FATAL message, flush all the log destinations, then toss
  // a signal for others to catch. We leave the logs in a state that
  // someone else can use them (as long as they flush afterwards)
  if (data_->severity_ == LOGGING_FATAL && exit_on_dfatal) {
    if (data_->first_fatal_) {
      // Store crash information so that it is accessible from within signal
      // handlers that may be invoked later.
      RecordCrashReason(&crash_reason);
      SetCrashReason(&crash_reason);

      // Store shortened fatal message for other logs and GWQ status
      const int copy =
          std::min<int>(data_->num_chars_to_log_, sizeof(fatal_message) - 1);
      memcpy(fatal_message, data_->message_text_, copy);
      fatal_message[copy] = '\0';
      fatal_time = data_->tick_counts_;
    }

    if (!absl::GetFlag(FLAGS_logtostderr)) {
      for (LogDestination* log : LogDestination::log_destinations_) {
        if (log)
          log->logger_->Write(true, 0, "", 0);
      }
    }

    // release the lock that our caller (directly or indirectly)
    // LogMessage::~LogMessage() grabbed so that signal handlers
    // can use the logging facility. Alternately, we could add
    // an entire unsafe logging interface to bypass locking
    // for signal handlers but this seems simpler.
    log_mutex.Unlock();
    LogDestination::WaitForSinks(data_);

    const char* message = "*** Check failure stack trace: ***\n";
    if (write(STDERR_FILENO, message, strlen(message)) < 0) {
      // Ignore errors.
    }
    Fail();
  }
}

void LogMessage::RecordCrashReason(CrashReason* reason) {
  reason->filename = fatal_msg_data_exclusive.fullname_;
  reason->line_number = fatal_msg_data_exclusive.line_;
  reason->message = fatal_msg_data_exclusive.message_text_ +
                    fatal_msg_data_exclusive.num_prefix_chars_;
  // Retrieve the stack trace, omitting the logging frames that got us here.
  reason->depth =
      absl::GetStackTrace(reason->stack, ARRAYSIZE(reason->stack), 4);
}

void LogMessage::Fail() {
  DumpStackTraceAndExit();
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SendToSink() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->sink_ != nullptr) {
    assert(data_->num_chars_to_log_ > 0 &&
           data_->message_text_[data_->num_chars_to_log_ - 1] == '\n');
    data_->sink_->send(
        data_->severity_, data_->fullname_, data_->basename_, data_->line_,
        data_->message_text_ + data_->num_prefix_chars_,
        (data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1),
        data_->tick_counts_);
  }
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SendToSinkAndLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  SendToSink();
  SendToLog();
}

// L >= log_mutex (callers must hold the log_mutex).
void LogMessage::SaveOrSendToLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->outvec_ != nullptr) {
    assert(data_->num_chars_to_log_ > 0 &&
           data_->message_text_[data_->num_chars_to_log_ - 1] == '\n');
    // Omit prefix of message and trailing newline when recording in outvec_.
    const char* start = data_->message_text_ + data_->num_prefix_chars_;
    int len = data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1;
    data_->outvec_->push_back(std::string(start, len));
  } else {
    SendToLog();
  }
}

void LogMessage::WriteToStringAndLog() EXCLUSIVE_LOCKS_REQUIRED(log_mutex) {
  if (data_->message_ != nullptr) {
    assert(data_->num_chars_to_log_ > 0 &&
           data_->message_text_[data_->num_chars_to_log_ - 1] == '\n');
    // Omit prefix of message and trailing newline when writing to message_.
    const char* start = data_->message_text_ + data_->num_prefix_chars_;
    int len = data_->num_chars_to_log_ - data_->num_prefix_chars_ - 1;
    data_->message_->assign(start, len);
  }
  SendToLog();
}

Logger* GetLogger(LogSeverity severity) {
  absl::MutexLock l(&log_mutex);
  return LogDestination::log_destination(severity)->logger_;
}

void SetLogger(LogSeverity severity, Logger* logger) {
  absl::MutexLock l(&log_mutex);
  LogDestination::log_destination(severity)->logger_ = logger;
}

// L < log_mutex.  Acquires and releases mutex_.
int64_t LogMessage::num_messages(int severity) {
  absl::MutexLock l(&log_mutex);
  return num_messages_[severity];
}

void FlushLogFiles(LogSeverity min_severity) {
  LogDestination::FlushLogFiles(min_severity);
}

void FlushLogFilesUnsafe(LogSeverity min_severity) {
  LogDestination::FlushLogFilesUnsafe(min_severity);
}

void SetLogDestination(LogSeverity severity, const char* base_filename) {
  LogDestination::SetLogDestination(severity, base_filename);
}

void SetLogSymlink(LogSeverity severity, const char* symlink_basename) {
  LogDestination::SetLogSymlink(severity, symlink_basename);
}

LogSink::~LogSink() = default;

void LogSink::WaitTillSent() {
  // noop default
}

std::string LogSink::ToString(LogSeverity severity,
                              const char* file,
                              int line,
                              const char* message,
                              size_t message_len,
                              uint64_t tick_counts) {
  std::ostringstream stream(std::string(message, message_len));
  stream.fill('0');

  stream << log_severity_name(severity)[0] << std::setw(6) << tick_counts << ' '
         << std::setfill(' ') << std::setw(5) << GetTID() << std::setfill('0')
         << ' ' << file << ':' << line << "] ";

  stream << std::string(message, message_len);
  return stream.str();
}

void AddLogSink(LogSink* destination) {
  LogDestination::AddLogSink(destination);
}

void RemoveLogSink(LogSink* destination) {
  LogDestination::RemoveLogSink(destination);
}

void SetLogFilenameExtension(const char* ext) {
  LogDestination::SetLogFilenameExtension(ext);
}

void SetStderrLogging(LogSeverity min_severity) {
  LogDestination::SetStderrLogging(min_severity);
}

void LogToStderr() {
  LogDestination::LogToStderr();
}

bool GetExitOnDFatal();
bool GetExitOnDFatal() {
  absl::MutexLock l(&log_mutex);
  return exit_on_dfatal;
}

// Determines whether we exit the program for a LOG(DFATAL) message in
// debug mode.  It does this by skipping the call to Fail/FailQuietly.
// This is intended for testing only.
//
// This can have some effects on LOG(FATAL) as well.  Failure messages
// are always allocated (rather than sharing a buffer), the crash
// reason is not recorded, the "gwq" status message is not updated,
// and the stack trace is not recorded.  The LOG(FATAL) *will* still
// exit the program.  Since this function is used only in testing,
// these differences are acceptable.
void SetExitOnDFatal(bool value);
void SetExitOnDFatal(bool value) {
  absl::MutexLock l(&log_mutex);
  exit_on_dfatal = value;
}

static void GetTempDirectories(std::vector<std::string>* list) {
  list->clear();
#ifdef OS_WIN
  // On windows we'll try to find a directory in this order:
  //   C:/Documents & Settings/whomever/TEMP (or whatever GetTempPath() is)
  //   C:/TMP/
  //   C:/TEMP/
  //   C:/WINDOWS/ or C:/WINNT/
  //   .
  char tmp[MAX_PATH];
  if (GetTempPathA(MAX_PATH, tmp))
    list->push_back(tmp);
  list->push_back("C:\\tmp\\");
  list->push_back("C:\\temp\\");
#else
  // Directories, in order of preference. If we find a dir that
  // exists, we stop adding other less-preferred dirs
  const char* candidates[] = {
      // Explicitly-supplied temp dirs
      getenv("TMPDIR"),
      getenv("TMP"),

      // If all else fails
      "/tmp",
  };

  for (const char* d : candidates) {
    if (!d)
      continue;  // Empty env var

    // Make sure we don't surprise anyone who's expecting a '/'
    std::string dstr = d;
    if (dstr[dstr.size() - 1] != '/') {
      dstr += "/";
    }
    list->push_back(dstr);

    struct stat statbuf;
    if (!stat(d, &statbuf) && S_ISDIR(statbuf.st_mode)) {
      // We found a dir that exists - we're done.
      return;
    }
  }

#endif
}

static std::vector<std::string>* logging_directories_list;

static std::once_flag logging_directories_list_once_flag;

const std::vector<std::string>& GetLoggingDirectories() {
  std::call_once(logging_directories_list_once_flag, []() {
    logging_directories_list = new std::vector<std::string>;

    if (!absl::GetFlag(FLAGS_log_dir).empty()) {
      // A dir was specified, we should use it
      logging_directories_list->push_back(absl::GetFlag(FLAGS_log_dir));
    } else {
      GetTempDirectories(logging_directories_list);
#ifdef OS_WIN
      char tmp[MAX_PATH];
      if (GetWindowsDirectoryA(tmp, MAX_PATH)) {
        logging_directories_list->push_back(tmp);
      }
      logging_directories_list->push_back(".\\");
#else
      logging_directories_list->push_back("./");
#endif
    }
  });
  return *logging_directories_list;
}

void GetExistingTempDirectories(std::vector<std::string>* list) {
  GetTempDirectories(list);
  std::vector<std::string>::iterator i_dir = list->begin();
  while (i_dir != list->end()) {
    // zero arg to access means test for existence; no constant
    // defined on windows
    if (access(i_dir->c_str(), 0)) {
      i_dir = list->erase(i_dir);
    } else {
      ++i_dir;
    }
  }
}

void TruncateLogFile(const char* path, int64_t limit, int64_t keep) {
#ifndef OS_WIN
  struct stat statbuf;
  const int kCopyBlockSize = 8 << 10;
  char copybuf[kCopyBlockSize];
  int64_t read_offset, write_offset;
  // Don't follow symlinks unless they're our own fd symlinks in /proc
  int flags = O_RDWR;
  // TODO(hamaji): Support other environments.
#ifdef OS_LINUX
  const char* procfd_prefix = "/proc/self/fd/";
  if (strncmp(procfd_prefix, path, strlen(procfd_prefix)))
    flags |= O_NOFOLLOW;
#endif

  int fd = open(path, flags);
  if (fd == -1) {
    if (errno == EFBIG) {
      // The log file in question has got too big for us to open. The
      // real fix for this would be to compile logging.cc (or probably
      // all of base/...) with -D_FILE_OFFSET_BITS=64 but that's
      // rather scary.
      // Instead just truncate the file to something we can manage
      if (truncate(path, 0) == -1) {
        PLOG(ERROR) << "Unable to truncate " << path;
      } else {
        LOG(ERROR) << "Truncated " << path << " due to EFBIG error";
      }
    } else {
      PLOG(ERROR) << "Unable to open " << path;
    }
    return;
  }

  if (fstat(fd, &statbuf) == -1) {
    PLOG(ERROR) << "Unable to fstat()";
    goto out_close_fd;
  }

  // See if the path refers to a regular file bigger than the
  // specified limit
  if (!S_ISREG(statbuf.st_mode))
    goto out_close_fd;
  if (statbuf.st_size <= limit)
    goto out_close_fd;
  if (statbuf.st_size <= keep)
    goto out_close_fd;

  // This log file is too large - we need to truncate it
  LOG(INFO) << "Truncating " << path << " to " << keep << " bytes";

  // Copy the last "keep" bytes of the file to the beginning of the file
  read_offset = statbuf.st_size - keep;
  write_offset = 0;
  int bytesin, bytesout;
  while ((bytesin = pread(fd, copybuf, sizeof(copybuf), read_offset)) > 0) {
    bytesout = pwrite(fd, copybuf, bytesin, write_offset);
    if (bytesout == -1) {
      PLOG(ERROR) << "Unable to write to " << path;
      break;
    } else if (bytesout != bytesin) {
      LOG(ERROR) << "Expected to write " << bytesin << ", wrote " << bytesout;
    }
    read_offset += bytesin;
    write_offset += bytesout;
  }
  if (bytesin == -1)
    PLOG(ERROR) << "Unable to read from " << path;

  // Truncate the remainder of the file. If someone else writes to the
  // end of the file after our last read() above, we lose their latest
  // data. Too bad ...
  if (ftruncate(fd, write_offset) == -1) {
    PLOG(ERROR) << "Unable to truncate " << path;
  }

out_close_fd:
  close(fd);
#else
  LOG(ERROR) << "No log truncation support.";
#endif
}

void TruncateStdoutStderr() {
#ifndef OS_WIN
  int64_t limit = MaxLogSize() << 20;
  int64_t keep = 1 << 20;
  TruncateLogFile("/proc/self/fd/1", limit, keep);
  TruncateLogFile("/proc/self/fd/2", limit, keep);
#else
  LOG(ERROR) << "No log truncation support.";
#endif
}

std::string StrError(int err) {
  char buf[100];
  safe_strerror_r(err, buf, sizeof(buf));
  if (buf[0] == '\000') {
    snprintf(buf, sizeof(buf), "Error number %d", err);
  }
  return buf;
}

LogMessageFatal::LogMessageFatal(const char* file, int line)
    : LogMessage(file, line, LOGGING_FATAL) {}

MSVC_PUSH_DISABLE_WARNING(4722)
LogMessageFatal::~LogMessageFatal() {
  Flush();
  LogMessage::Fail();
}
MSVC_POP_WARNING()

// Broken out from logging.cc by Soren Lassen
// logging_unittest.cc covers the functionality herein

ABSL_FLAG(int32_t, v, DEFAULT_VERBOSE_LEVEL, "verboselevel");

ABSL_FLAG(std::string,
          vmodule,
          "",
          "per-module verbose level."
          " Argument is a comma-separated list of <module name>=<log level>."
          " <module name> is a glob pattern, matched against the filename base"
          " (that is, name ignoring .cc/.h./-inl.h)."
          " <log level> overrides any value given by --v.");

#define ANNOTATE_BENIGN_RACE(address, description)

bool SafeFNMatch_(const char* pattern,
                  size_t patt_len,
                  const char* str,
                  size_t str_len);

// Implementation of fnmatch that does not need 0-termination
// of arguments and does not allocate any memory,
// but we only support "*" and "?" wildcards, not the "[...]" patterns.
// It's not a static function for the unittest.
bool SafeFNMatch_(const char* pattern,
                  size_t patt_len,
                  const char* str,
                  size_t str_len) {
  size_t p = 0;
  size_t s = 0;
  while (true) {
    if (p == patt_len && s == str_len)
      return true;
    if (p == patt_len)
      return false;
    if (s == str_len)
      return p + 1 == patt_len && pattern[p] == '*';
    if (pattern[p] == str[s] || pattern[p] == '?') {
      p += 1;
      s += 1;
      continue;
    }
    if (pattern[p] == '*') {
      if (p + 1 == patt_len)
        return true;
      do {
        if (SafeFNMatch_(pattern + (p + 1), patt_len - (p + 1), str + s,
                         str_len - s)) {
          return true;
        }
        s += 1;
      } while (s != str_len);
      return false;
    }
    return false;
  }
}

// List of per-module log levels from FLAGS_vmodule.
// Once created each element is never deleted/modified
// except for the vlog_level: other threads will read VModuleInfo blobs
// w/o locks and we'll store pointers to vlog_level at VLOG locations
// that will never go away.
// We can't use an STL struct here as we wouldn't know
// when it's safe to delete/update it: other threads need to use it w/o locks.
struct VModuleInfo {
  std::string module_pattern;
  mutable absl::Flag<int32_t>*
      vlog_level;  // Conceptually this is an AtomicWord, but it's
                   // too much work to use AtomicWord type here
                   // w/o much actual benefit.
  const VModuleInfo* next;
};

// This protects the following global variables.
static absl::Mutex vmodule_lock;
// Pointer to head of the VModuleInfo list.
// It's a map from module pattern to logging level for those module(s).
static VModuleInfo* vmodule_list = nullptr;

// Boolean initialization flag.
static bool inited_vmodule = false;

// L >= vmodule_lock.
static void VLOG2Initializer() {
  vmodule_lock.AssertHeld();
  // Can now parse --vmodule flag and initialize mapping of module-specific
  // logging levels.
  inited_vmodule = false;
  const std::string vmodule_data = absl::GetFlag(FLAGS_vmodule);
  absl::string_view vmodule(vmodule_data);
  const char* sep;
  VModuleInfo* head = nullptr;
  VModuleInfo* tail = nullptr;
  while ((sep = strchr(vmodule.data(), '=')) != nullptr) {
    std::string pattern(vmodule.data(),
                        static_cast<size_t>(sep - vmodule.data()));
    int module_level;
    if (sscanf(sep, "=%d", &module_level) == 1) {
      VModuleInfo* info = new VModuleInfo;
      info->module_pattern = pattern;
      absl::SetFlag(info->vlog_level, module_level);
      if (head) {
        tail->next = info;
      } else {
        head = info;
      }
      tail = info;
    }
    // Skip past this entry
    vmodule = strchr(sep, ',');
    if (vmodule == nullptr)
      break;
    vmodule.remove_prefix(1);
  }
  if (head) {  // Put them into the list at the head:
    tail->next = vmodule_list;
    vmodule_list = head;
  }
  inited_vmodule = true;
}

// This can be called very early, so we use SpinLock here.
int SetVLOGLevel(const char* module_pattern, int log_level) {
  int result = absl::GetFlag(FLAGS_v);
  size_t const pattern_len = strlen(module_pattern);
  bool found = false;
  {
    absl::MutexLock l(&vmodule_lock);  // protect whole read-modify-write
    for (const VModuleInfo* info = vmodule_list; info != nullptr;
         info = info->next) {
      if (info->module_pattern == module_pattern) {
        if (!found) {
          result = absl::GetFlag(*info->vlog_level);
          found = true;
        }
        absl::SetFlag(info->vlog_level, log_level);
      } else if (!found && SafeFNMatch_(info->module_pattern.c_str(),
                                        info->module_pattern.size(),
                                        module_pattern, pattern_len)) {
        result = absl::GetFlag(*info->vlog_level);
        found = true;
      }
    }
    if (!found) {
      VModuleInfo* info = new VModuleInfo;
      info->module_pattern = module_pattern;
      absl::SetFlag(info->vlog_level, log_level);
      info->next = vmodule_list;
      vmodule_list = info;
    }
  }
  fprintf(stderr, "Set VLOG level for \"%s\" to %d", module_pattern, log_level);
  return result;
}

// NOTE: Individual VLOG statements cache the integer log level pointers.
// NOTE: This function must not allocate memory or require any locks.
bool InitVLOG3__(absl::Flag<int32_t>** site_flag,
                 absl::Flag<int32_t>* level_default,
                 const char* fname,
                 int32_t verbose_level) {
  absl::MutexLock l(&vmodule_lock);
  bool read_vmodule_flag = inited_vmodule;
  if (!read_vmodule_flag) {
    VLOG2Initializer();
  }

  // protect the errno global in case someone writes:
  // VLOG(..) << "The last error was " << strerror(errno)
  int old_errno = errno;

  // site_default normally points to FLAGS_v
  absl::Flag<int32_t>* site_flag_value = level_default;

  // Get basename for file
  const char* base = strrchr(fname, '/');

#ifdef OS_WIN
  if (!base) {
    base = strrchr(fname, '\\');
  }
#endif

  base = base ? (base + 1) : fname;
  const char* base_end = strchr(base, '.');
  size_t base_length =
      base_end ? static_cast<size_t>(base_end - base) : strlen(base);

  // Trim out trailing "-inl" if any
  if (base_length >= 4 && (memcmp(base + base_length - 4, "-inl", 4) == 0)) {
    base_length -= 4;
  }

  // TODO: Trim out _unittest suffix?  Perhaps it is better to have
  // the extra control and just leave it there.

  // find target in vector of modules, replace site_flag_value with
  // a module-specific verbose level, if any.
  for (const VModuleInfo* info = vmodule_list; info != nullptr;
       info = info->next) {
    if (SafeFNMatch_(info->module_pattern.c_str(), info->module_pattern.size(),
                     base, base_length)) {
      site_flag_value = info->vlog_level;
      // value at info->vlog_level is now what controls
      // the VLOG at the caller site forever
      break;
    }
  }

  // Cache the vlog value pointer if --vmodule flag has been parsed.
  ANNOTATE_BENIGN_RACE(site_flag,
                       "*site_flag may be written by several threads,"
                       " but the value will be the same");
  if (read_vmodule_flag)
    *site_flag = site_flag_value;

  // restore the errno in case something recoverable went wrong during
  // the initialization of the VLOG mechanism (see above note "protect the..")
  errno = old_errno;
  return absl::GetFlag(*site_flag_value) >= verbose_level;
}

// Broken out from utilities.cc
// Author: Shinichiro Hamaji

ABSL_FLAG(bool,
          symbolize_stacktrace,
          true,
          "Symbolize the stack trace in the tombstone");

typedef void DebugWriter(const char*, void*);

// The %p field width for printf() functions is two characters per byte.
// For some environments, add two extra bytes for the leading "0x".
static const int kPrintfPointerFieldWidth = 2 + 2 * sizeof(void*);

static void DebugWriteToStderr(const char* data, void*) {
  // This one is signal-safe.
  if (write(STDERR_FILENO, data, strlen(data)) < 0) {
    // Ignore errors.
  }
}

static void DebugWriteToString(const char* data, void* arg) {
  reinterpret_cast<std::string*>(arg)->append(data);
}

// Print a program counter and its symbol name.
static void DumpPCAndSymbol(DebugWriter* writerfn,
                            void* arg,
                            void* pc,
                            const char* const prefix) {
  char tmp[1024];
  const char* symbol = "(unknown)";
  // Symbolizes the previous address of pc because pc may be in the
  // next function.  The overrun happens when the function ends with
  // a call to a function annotated noreturn (e.g. CHECK).
  if (absl::Symbolize(reinterpret_cast<char*>(pc) - 1, tmp, sizeof(tmp))) {
    symbol = tmp;
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s@ %*p  %s\n", prefix, kPrintfPointerFieldWidth,
           pc, symbol);
  writerfn(buf, arg);
}

static void DumpPC(DebugWriter* writerfn,
                   void* arg,
                   void* pc,
                   const char* const prefix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%s@ %*p\n", prefix, kPrintfPointerFieldWidth, pc);
  writerfn(buf, arg);
}

// Dump current stack trace as directed by writerfn
static void DumpStackTrace(int skip_count, DebugWriter* writerfn, void* arg) {
  // Print stack trace
  void* stack[32];
  int depth = absl::GetStackTrace(stack, ARRAYSIZE(stack), skip_count + 1);
  for (int i = 0; i < depth; i++) {
    if (absl::GetFlag(FLAGS_symbolize_stacktrace)) {
      DumpPCAndSymbol(writerfn, arg, stack[i], "    ");
    } else {
      DumpPC(writerfn, arg, stack[i], "    ");
    }
  }
}

void DumpStackTraceAndExit() {
  DumpStackTrace(1, DebugWriteToStderr, nullptr);

  // TODO(hamaji): Use signal instead of sigaction?
  // Set the default signal handler for SIGABRT, to avoid invoking our
  // own signal handler installed by InstallFailureSignalHandler().
#if defined(OS_POSIX)
  struct sigaction sig_action;
  memset(&sig_action, 0, sizeof(sig_action));
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_handler = SIG_DFL;
  sigaction(SIGABRT, &sig_action, nullptr);
#elif defined(OS_WIN)
  signal(SIGABRT, SIG_DFL);
#endif  // defined(OS_POSIX)

  abort();
}

#ifdef OS_WIN
struct timeval {
  long tv_sec, tv_usec;
};

// Based on:
// http://www.google.com/codesearch/p?hl=en#dR3YEbitojA/os_win32.c&q=GetSystemTimeAsFileTime%20license:bsd
// See COPYING for copyright information.
static int gettimeofday(struct timeval* tv, void* tz) {
#define EPOCHFILETIME (116444736000000000ULL)
  FILETIME ft;
  LARGE_INTEGER li;
  uint64_t tt;

  GetSystemTimeAsFileTime(&ft);
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;
  tt = (li.QuadPart - EPOCHFILETIME) / 10;
  tv->tv_sec = tt / 1000000;
  tv->tv_usec = tt % 1000000;

  return 0;
}
#endif

int64_t CycleClock_Now() {
  // TODO(hamaji): temporary impementation - it might be too slow.
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

int64_t UsecToCycles(int64_t usec) {
  return usec;
}

WallTime WallTime_Now() {
  // Now, cycle clock is retuning microseconds since the epoch.
  return static_cast<double>(CycleClock_Now()) * 0.000001;
}

static int32_t g_main_thread_pid = GetPID();
int32_t GetMainThreadPid() {
  return g_main_thread_pid;
}

bool PidHasChanged() {
  int32_t pid = getpid();
  if (g_main_thread_pid == pid) {
    return false;
  }
  g_main_thread_pid = pid;
  return true;
}

// Keep the same implementation with chromium

#if defined(OS_LINUX) && !defined(OS_ANDROID)

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally. Clear
// the stored value after a fork() because forking changes the thread
// id. Forking without going through fork() (e.g. clone()) is not
// supported, but there is no known usage. Using thread_local is
// fine here (despite being banned) since it is going to be allowed
// but is blocked on a clang bug for Mac (https://crbug.com/829078)
// and we can't use ThreadLocalStorage because of re-entrancy due to
// CHECK/DCHECKs.
thread_local pid_t g_thread_id = -1;

static void ClearTidCache() {
  g_thread_id = -1;
}

class InitAtFork {
 public:
  InitAtFork() { pthread_atfork(nullptr, nullptr, ClearTidCache); }
};

#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

pid_t GetPID() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_POSIX)
  return getpid();
#elif defined(OS_WIN)
  return GetCurrentProcessId();
#else
#error
#endif
}

pid_t GetTID() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_APPLE)
  return pthread_mach_thread_np(pthread_self());
  // On Linux and MacOSX, we try to use gettid().
#elif defined(OS_LINUX) && !defined(OS_ANDROID)
  static InitAtFork init_at_fork;
  if (g_thread_id == -1) {
    g_thread_id = syscall(__NR_gettid);
  } else {
    DCHECK_EQ(g_thread_id, syscall(__NR_gettid))
        << "Thread id stored in TLS is different from thread id returned by "
           "the system. It is likely that the process was forked without going "
           "through fork().";
  }
  return g_thread_id;
#elif defined(OS_ANDROID)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif defined(OS_WIN)
  return GetCurrentThreadId();
#else
  // If none of the techniques above worked, we use pthread_self().
  return (pid_t)(uintptr_t)pthread_self();
#endif
}

const char* const_basename(const char* filepath) {
  const char* base = strrchr(filepath, '/');
#ifdef OS_WIN  // Look for either path separator in Windows
  if (!base)
    base = strrchr(filepath, '\\');
#endif
  return base ? (base + 1) : filepath;
}

static std::string g_my_user_name;
static std::once_flag g_my_user_name_once_flag;
static void MyUserNameInitializer();

const std::string& MyUserName() {
  std::call_once(g_my_user_name_once_flag, []() { MyUserNameInitializer(); });
  return g_my_user_name;
}

void MyUserNameInitializer() {
  // TODO(hamaji): Probably this is not portable.
#if defined(OS_WIN)
  const char* user = getenv("USERNAME");
#else
  const char* user = getenv("USER");
#endif
  if (user != nullptr) {
    g_my_user_name = user;
  } else {
#if defined(HAVE_PWD_H) && !defined(OS_WIN)
    struct passwd pwd;
    struct passwd* result = nullptr;
    char buffer[1024] = {'\0'};
    uid_t uid = geteuid();
    int pwuid_res = getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &result);
    if (pwuid_res == 0 && result) {
      g_my_user_name = pwd.pw_name;
    } else {
      snprintf(buffer, sizeof(buffer), "uid%d", uid);
      g_my_user_name = buffer;
    }
#endif
    if (g_my_user_name.empty()) {
      g_my_user_name = "invalid-user";
    }
  }
}

void DumpStackTraceToString(std::string* stacktrace) {
  DumpStackTrace(1, DebugWriteToString, stacktrace);
}

// We use an atomic operation to prevent problems with calling CrashReason
// from inside the Mutex implementation (potentially through RAW_CHECK).
static std::atomic<const CrashReason*> g_reason;

bool SetCrashReason(const CrashReason* r) {
  const CrashReason* empty_reason = nullptr;
  return g_reason.compare_exchange_strong(empty_reason, r);
}

// logging.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

bool ShouldCreateLogMessage(LogSeverity severity) {
  if (severity < absl::GetFlag(FLAGS_minloglevel))
    return false;

  return true;
}

#if defined(OS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if defined(OS_WIN)
  return ::GetLastError();
#elif defined(OS_POSIX)
  return errno;
#endif
}

std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if defined(OS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             sizeof(msgbuf) / sizeof(msgbuf[0]), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    return CollapseWhitespaceASCII(msgbuf, true) +
           StringPrintf(" (0x%lX)", error_code);
  }
  return StringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                      GetLastError(), error_code);
#elif defined(OS_POSIX)
  return safe_strerror(error_code) + StringPrintf(" (%d)", error_code);
#endif  // defined(OS_WIN)
}

#if defined(OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  Alias(&last_error);
}
#elif defined(OS_POSIX)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  Alias(&last_error);
}
#endif  // defined(OS_WIN)

void RawLog(int level, const char* message) {
  if (level >= absl::GetFlag(FLAGS_minloglevel) && message) {
    size_t bytes_written = 0;
    const size_t message_len = strlen(message);
    int rv;
    while (bytes_written < message_len) {
      rv = HANDLE_EINTR(write(STDERR_FILENO, message + bytes_written,
                              message_len - bytes_written));
      if (rv < 0) {
        // Give up, nothing we can do now.
        break;
      }
      bytes_written += rv;
    }

    if (message_len > 0 && message[message_len - 1] != '\n') {
      do {
        rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
        if (rv < 0) {
          // Give up, nothing we can do now.
          break;
        }
      } while (rv != 1);
    }
  }

  if (level == LOGGING_FATAL)
    BreakDebuggerAsyncSafe();
}
