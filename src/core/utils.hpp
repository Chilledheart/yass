// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS

#include <stdint.h>
#include <string>
#include <string_view>
#include <thread>
#include <wchar.h>
#include <optional>
#include <iosfwd>

#include <base/compiler_specific.h>
#include <base/strings/sys_string_conversions.h>
#include <base/files/platform_file.h>
#include <base/files/memory_mapped_file.h>

#ifdef __ANDROID__
extern std::string a_cache_dir;
extern std::string a_data_dir;
typedef int (*OpenApkAssetType)(const std::string&, gurl_base::MemoryMappedFile::Region*);
extern OpenApkAssetType a_open_apk_asset;
#endif

#ifdef __OHOS__
extern std::string h_cache_dir;
extern std::string h_data_dir;
#endif

using gurl_base::PlatformFile;

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

bool SetCurrentThreadPriority(ThreadPriority priority);

bool SetCurrentThreadName(const std::string& name);

uint64_t GetMonotonicTime();

#define NS_PER_SECOND (1000 * 1000 * 1000)

namespace internal {
#ifdef _WIN32
  using fd_t = HANDLE;
#else
  using fd_t = int;
#endif
} // namespace internal

bool IsProgramConsole(internal::fd_t fd);

bool SetUTF8Locale();

std::optional<int> StringToInteger(const std::string& value);
std::optional<unsigned> StringToIntegerU(const std::string& value);
std::optional<int64_t> StringToInteger64(const std::string& value);
std::optional<uint64_t> StringToIntegerU64(const std::string& value);

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
#ifdef _WIN32
// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
using gurl_base::SysMultiByteToWide;
using gurl_base::SysWideToMultiByte;
#endif // _WIN32

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
std::string_view Dirname(std::string_view path);

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
std::string_view Basename(std::string_view path);

std::string ExpandUser(const std::string& file_path);
#ifdef _WIN32
/* path_len should be the string length plus the terminating null character*/
std::wstring ExpandUserFromString(const wchar_t* path, size_t path_len);
#endif

bool GetExecutablePath(std::string* exe_path);
#ifdef _WIN32
bool GetExecutablePath(std::wstring* exe_path);
#endif

void SetExecutablePath(const std::string& exe_path);
#ifdef _WIN32
void SetExecutablePath(const std::wstring& exe_path);
#endif

bool GetTempDir(std::string *path);
#ifdef _WIN32
bool GetTempDir(std::wstring *path);
#endif

std::string GetHomeDir();
#ifdef _WIN32
std::wstring GetHomeDirW();
#endif

bool Net_ipv6works();

#ifdef _MSC_VER
// https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
using ssize_t = ptrdiff_t;
#endif

ssize_t ReadFileToBuffer(const std::string& path, char* buf, size_t buf_len);
ssize_t WriteFileWithBuffer(const std::string& path, std::string_view buf);
PlatformFile OpenReadFile(const std::string &path);
#ifdef _WIN32
PlatformFile OpenReadFile(const std::wstring& path);
#endif

#ifdef HAVE_TCMALLOC
void PrintTcmallocStats();
#endif

#ifdef __APPLE__
#if BUILDFLAG(IS_IOS)
#include <MacTypes.h>
#else
#include <libkern/OSTypes.h>
#endif
std::string DescriptionFromOSStatus(OSStatus err);
#endif // __APPLE__

void HumanReadableByteCountBin(std::ostream* ss, uint64_t bytes);
#ifdef _WIN32
void HumanReadableByteCountBin(std::wostream* ss, uint64_t bytes);
#endif

#endif  // YASS_UTILS
