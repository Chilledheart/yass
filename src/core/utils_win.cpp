// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef _WIN32

#include <windows.h>

uint64_t GetMonotonicTime() {
  static LARGE_INTEGER StartTime, Frequency;
  static bool started;

  LARGE_INTEGER CurrentTime, ElapsedNanoseconds;

  if (!started) {
    if (!QueryPerformanceFrequency(&Frequency)) {
      LOG(WARNING) << "QueryPerformanceFrequency failed";
      return 0;
    }
    if (!QueryPerformanceCounter(&StartTime)) {
      LOG(WARNING) << "QueryPerformanceCounter failed";
      return 0;
    }
    started = true;
  }
  // Activity to be timed
  if (!QueryPerformanceCounter(&CurrentTime)) {
    LOG(WARNING) << "QueryPerformanceCounter failed";
    return 0;
  }

  ElapsedNanoseconds.QuadPart = CurrentTime.QuadPart - StartTime.QuadPart;

  //
  // We now have the elapsed number of ticks, along with the
  // number of ticks-per-second. We use these values
  // to convert to the number of elapsed microseconds.
  // To guard against loss-of-precision, we convert
  // to microseconds *before* dividing by ticks-per-second.
  //

  ElapsedNanoseconds.QuadPart =
      static_cast<double>(ElapsedNanoseconds.QuadPart) * NS_PER_SECOND /
      Frequency.QuadPart;

  return ElapsedNanoseconds.QuadPart;
}

// borrowed from sys_string_conversions_win.cc

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return SysWideToMultiByte(wide, CP_UTF8);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(absl::string_view utf8) {
  return SysMultiByteToWide(utf8, CP_UTF8);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToMultiByte(wide, CP_ACP);
}

std::wstring SysNativeMBToWide(absl::string_view native_mb) {
  return SysMultiByteToWide(native_mb, CP_ACP);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysMultiByteToWide(absl::string_view mb, uint32_t code_page) {
  int mb_length = static_cast<int>(mb.length());
  // Note that, if cbMultiByte is 0, the function fails.
  if (mb_length == 0)
    return std::wstring();

  // Compute the length of the buffer.
  int charcount = MultiByteToWideChar(
      code_page /* CodePage */, 0 /* dwFlags */, mb.data() /* lpMultiByteStr */,
      mb_length /* cbMultiByte */, nullptr /* lpWideCharStr */,
      0 /* cchWideChar */);
  // The function returns 0 if it does not succeed.
  if (charcount == 0)
    return std::wstring();

  // If the function succeeds and cchWideChar is 0,
  // the return value is the required size, in characters,
  std::wstring wide;
  wide.resize(charcount);
  MultiByteToWideChar(code_page /* CodePage */, 0 /* dwFlags */,
                      mb.data() /* lpMultiByteStr */,
                      mb_length /* cbMultiByte */, &wide[0] /* lpWideCharStr */,
                      charcount /* cchWideChar */);

  return wide;
}

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page) {
  int wide_length = static_cast<int>(wide.length());
  // If cchWideChar is set to 0, the function fails.
  if (wide_length == 0)
    return std::string();

  // Compute the length of the buffer we'll need.
  int charcount = WideCharToMultiByte(
      code_page /* CodePage */, 0 /* dwFlags */,
      wide.data() /* lpWideCharStr */, wide_length /* cchWideChar */,
      nullptr /* lpMultiByteStr */, 0 /* cbMultiByte */,
      nullptr /* lpDefaultChar */, nullptr /* lpUsedDefaultChar */);
  // The function returns 0 if it does not succeed.
  if (charcount == 0)
    return std::string();
  // If the function succeeds and cbMultiByte is 0, the return value is
  // the required size, in bytes, for the buffer indicated by lpMultiByteStr.
  std::string mb;
  mb.resize(charcount);

  WideCharToMultiByte(
      code_page /* CodePage */, 0 /* dwFlags */,
      wide.data() /* lpWideCharStr */, wide_length /* cchWideChar */,
      &mb[0] /* lpMultiByteStr */, charcount /* cbMultiByte */,
      nullptr /* lpDefaultChar */, nullptr /* lpUsedDefaultChar */);

  return mb;
}

#endif  // _WIN32
