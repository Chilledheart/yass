// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS

#include <absl/status/statusor.h>
#include <absl/strings/string_view.h>
#include <cstdint>
#include <string>

uint64_t GetMonotonicTime();
#define NS_PER_SECOND (1000 * 1000 * 1000)

absl::StatusOr<int32_t> StringToInteger(absl::string_view value);

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
std::string SysWideToUTF8(const std::wstring& wide);
std::wstring SysUTF8ToWide(absl::string_view utf8);

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
std::wstring SysMultiByteToWide(absl::string_view mb, uint32_t code_page);

std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page);

// Windows-specific ------------------------------------------------------------
#ifdef _WIN32
// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
std::string SysWideToNativeMB(const std::wstring& wide);

std::wstring SysNativeMBToWide(absl::string_view native_mb);
#endif

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
inline absl::string_view Dirname(absl::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == absl::string_view::npos) {
    return path.empty() ? "/" : path.substr(0, 1);
  }

  auto last_slash_pos =
      path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory.
  if (last_slash_pos == absl::string_view::npos) {
    return ".";
  }

  // trim the extra trailing slash
  first_non_slash_at_end_pos =
      path.find_last_not_of(kSeparators, last_slash_pos);

  // path is in the root directory
  if (first_non_slash_at_end_pos == absl::string_view::npos) {
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
inline absl::string_view Basename(absl::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == absl::string_view::npos) {
    return path.empty() ? "" : path.substr(0, 1);
  }

  auto last_slash_pos =
      path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory
  if (last_slash_pos == absl::string_view::npos) {
    return path.substr(0, first_non_slash_at_end_pos + 1);
  }

  // path is in the root directory
  return path.substr(last_slash_pos + 1,
                     first_non_slash_at_end_pos - last_slash_pos);
}

#endif  // YASS_UTILS
