// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS

#ifdef _MSC_VER
#pragma push
// constructor is not implicitly called
#pragma warning(disable : 4582)
// destructor is not implicitly called
#pragma warning(disable : 4583)
#endif  // _MSC_VER
#include <absl/status/statusor.h>
#ifdef _MSC_VER
#pragma pop
#endif  // _MSC_VER

#include <stdint.h>
#include <string>
#include <thread>
#include <wchar.h>

#include "core/compiler_specific.hpp"
#include "base/strings/sys_string_conversions.h"

#if defined(OS_APPLE)
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include "core/scoped_cftyperef.hpp"
#endif  // defined(OS_APPLE)

// Valid values for priority of Thread::Options and SimpleThread::Options, and
// SetCurrentThreadPriority(), listed in increasing order of importance.
enum class ThreadPriority : int {
  // Suitable for threads that shouldn't disrupt high priority work.
  BACKGROUND,
  // Default priority level.
  NORMAL,
  // Suitable for threads which generate data for the display (at ~60Hz).
  ABOVE_NORMAL,
  // Suitable for low-latency, glitch-resistant audio.
  TIME_CRITICAL,
};

bool SetThreadPriority(std::thread::native_handle_type handle,
                       ThreadPriority priority);

bool SetThreadName(std::thread::native_handle_type handle,
                   const std::string& name);

// Lock memory to avoid page fault
bool MemoryLockAll();

uint64_t GetMonotonicTime();

#define NS_PER_SECOND (1000 * 1000 * 1000)

bool IsProgramConsole();

bool SetUTF8Locale();

absl::StatusOr<int32_t> StringToInteger(const std::string& value);

#ifdef _WIN32
bool EnableSecureDllLoading();

void GetWindowsVersion(int* major, int* minor, int* build_number, int* os_type);

bool IsWindowsVersionBNOrGreater(int wMajorVersion,
                                 int wMinorVersion,
                                 int wBuildNumber);
#endif

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
using gurl_base::SysWideToUTF8;
using gurl_base::SysUTF8ToWide;

// Converts between wide and the system multi-byte representations of a string.
// DANGER: This will lose information and can change (on Windows, this can
// change between reboots).
using gurl_base::SysWideToNativeMB;
using gurl_base::SysNativeMBToWide;

// Windows-specific ------------------------------------------------------------
#ifdef OS_WIN
// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
using gurl_base::SysMultiByteToWide;
using gurl_base::SysWideToMultiByte;
#endif // OS_WIN

// Mac-specific ----------------------------------------------------------------
#if defined(OS_APPLE)

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class NSFont;
@class UIFont;
#else  // __OBJC__
#include <CoreFoundation/CoreFoundation.h>
class NSBundle;
class NSFont;
class NSString;
class UIFont;
#endif  // __OBJC__

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include <CoreText/CoreText.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

// Converts between STL strings and CFStringRefs/NSStrings.

// Creates a string, and returns it with a refcount of 1. You are responsible
// for releasing it. Returns NULL on failure.
ScopedCFTypeRef<CFStringRef> SysUTF8ToCFStringRef(std::string_view utf8);
ScopedCFTypeRef<CFStringRef> SysUTF16ToCFStringRef(const std::u16string& utf16);

// Same, but returns an autoreleased NSString.
NSString* SysUTF8ToNSString(std::string_view utf8);
NSString* SysUTF16ToNSString(const std::u16string& utf16);

// Converts a CFStringRef to an STL string. Returns an empty string on failure.
std::string SysCFStringRefToUTF8(CFStringRef ref);
std::u16string SysCFStringRefToUTF16(CFStringRef ref);

// Same, but accepts NSString input. Converts nil NSString* to the appropriate
// string type of length 0.
std::string SysNSStringToUTF8(NSString* ref);
std::u16string SysNSStringToUTF16(NSString* ref);

#endif  // defined(OS_APPLE)

extern const char kSeparators[];

// A portable interface that returns the dirname of the filename passed as an
// argument. It is similar to dirname(3)
// <https://linux.die.net/man/3/dirname>.
// For example:
//     Dirname("a/b/prog/file.cc")
// returns "a/b/prog"
//     Dirname("a/b/prog//")
// returns "a/b"
//     Dirname("file.cc")
// returns "."
//     Dirname("/file.cc")
// returns "/"
//     Dirname("//file.cc")
// returns "/"
//     Dirname("/dir//file.cc")
// returns "/dir"
//
// TODO: handle with driver letter under windows
inline std::string_view Dirname(std::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.empty() ? "/" : path.substr(0, 1);
  }

  auto last_slash_pos =
      path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory.
  if (last_slash_pos == std::string_view::npos) {
    return ".";
  }

  // trim the extra trailing slash
  first_non_slash_at_end_pos =
      path.find_last_not_of(kSeparators, last_slash_pos);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.substr(0, 1);
  }

  return path.substr(0, first_non_slash_at_end_pos + 1);
}

// A portable interface that returns the basename of the filename passed as an
// argument. It is similar to basename(3)
// <https://linux.die.net/man/3/basename>.
// For example:
//     Basename("a/b/prog/file.cc")
// returns "file.cc"
//     Basename("a/b/prog//")
// returns "prog"
//     Basename("file.cc")
// returns "file.cc"
//     Basename("/file.cc")
// returns "file.cc"
//     Basename("//file.cc")
// returns "file.cc"
//     Basename("/dir//file.cc")
// returns "file.cc"
//     Basename("////")
// returns "/"
//     Basename("c/")
// returns "c"
//     Basename("/a/b/c")
// returns "c"
//
// TODO: handle with driver letter under windows
inline std::string_view Basename(std::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.empty() ? "" : path.substr(0, 1);
  }

  auto last_slash_pos =
      path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory
  if (last_slash_pos == std::string_view::npos) {
    return path.substr(0, first_non_slash_at_end_pos + 1);
  }

  // path is in the root directory
  return path.substr(last_slash_pos + 1,
                     first_non_slash_at_end_pos - last_slash_pos);
}

std::string ExpandUser(const std::string& file_path);
#ifdef _WIN32
/* path_len should be the string length plus the terminating null character*/
std::wstring ExpandUserFromString(const wchar_t* path, size_t path_len);
#endif

bool GetExecutablePath(std::string* exe_path);
#ifdef _WIN32
bool GetExecutablePathW(std::wstring* exe_path);
#endif

void SetExecutablePath(const std::string& exe_path);
#ifdef _WIN32
void SetExecutablePathW(const std::wstring& exe_path);
#endif

bool Net_ipv6works();

#ifdef _MSC_VER
// https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
using ssize_t = ptrdiff_t;
#endif

ssize_t ReadFileToBuffer(const std::string& path, char* buf, size_t buf_len);
ssize_t WriteFileWithBuffer(const std::string& path, const char* buf, size_t buf_len);

#ifdef HAVE_TCMALLOC
void PrintTcmallocStats();
#endif

#endif  // YASS_UTILS
