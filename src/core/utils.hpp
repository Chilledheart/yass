// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS

#include <absl/status/statusor.h>
#include <absl/strings/string_view.h>
#include <cstdint>
#include <string>

uint64_t GetMonotonicTime();
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

#define NS_PER_SECOND (1000 * 1000 * 1000)

#endif  // YASS_UTILS
