// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/logging.hpp"

#ifdef _WIN32

#include <windows.h>

#if _WIN32_WINNT > 0x0601
#include <processthreadsapi.h>
#endif

// The most common value returned by ::GetThreadPriority() after background
// thread mode is enabled on Windows 7.
constexpr int kWin7BackgroundThreadModePriority = 4;

#if 0
// Value sometimes returned by ::GetThreadPriority() after thread priority is
// set to normal on Windows 7.
constexpr int kWin7NormalPriority = 3;

// These values are sometimes returned by ::GetThreadPriority().
constexpr int kWinNormalPriority1 = 5;
constexpr int kWinNormalPriority2 = 6;
#endif

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2022
const DWORD kVCThreadNameException = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

// The SetThreadDescription API was brought in version 1607 of Windows 10.
typedef HRESULT(WINAPI* PFNSETTHREADDESCRIPTION)(HANDLE hThread,
                                                 PCWSTR lpThreadDescription);

namespace {
// This function has try handling, so it is separated out of its caller.
void SetNameInternal(DWORD thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

  // https://docs.microsoft.com/en-us/windows/win32/debug/using-an-exception-handler
#pragma warning(push)
#pragma warning(disable: 6320 6322)
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<ULONG_PTR*>(&info));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
}

} // namespace

bool SetThreadPriority(std::thread::native_handle_type handle,
                       ThreadPriority priority) {
  int desired_priority = THREAD_PRIORITY_ERROR_RETURN;
  switch (priority) {
    case ThreadPriority::BACKGROUND:
      // Using THREAD_MODE_BACKGROUND_BEGIN instead of THREAD_PRIORITY_LOWEST
      // improves input latency and navigation time. See
      // https://docs.google.com/document/d/16XrOwuwTwKWdgPbcKKajTmNqtB4Am8TgS9GjbzBYLc0
      //
      // MSDN recommends THREAD_MODE_BACKGROUND_BEGIN for threads that perform
      // background work, as it reduces disk and memory priority in addition to
      // CPU priority.
      /* Windows Server 2003:  This value is not supported. */
      desired_priority = THREAD_MODE_BACKGROUND_BEGIN;
      break;
    case ThreadPriority::NORMAL:
      desired_priority = THREAD_PRIORITY_NORMAL;
      break;
    case ThreadPriority::ABOVE_NORMAL:
      desired_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case ThreadPriority::TIME_CRITICAL:
      desired_priority = THREAD_PRIORITY_TIME_CRITICAL;
      break;
    default:
      NOTREACHED() << "Unknown priority.";
      break;
  }
  DCHECK_NE(desired_priority, THREAD_PRIORITY_ERROR_RETURN);

  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadpriority
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
  BOOL ret = ::SetThreadPriority(handle, desired_priority);

  if (priority == ThreadPriority::BACKGROUND) {
    // In a background process, THREAD_MODE_BACKGROUND_BEGIN lowers the memory
    // and I/O priorities but not the CPU priority (kernel bug?). Use
    // THREAD_PRIORITY_LOWEST to also lower the CPU priority.
    // https://crbug.com/901483

    desired_priority = ::GetThreadPriority(handle);
    // Negative values represent a background priority. We have observed -3, -4,
    // -6 when THREAD_MODE_BACKGROUND_* is used. THREAD_PRIORITY_IDLE,
    // THREAD_PRIORITY_LOWEST and THREAD_PRIORITY_BELOW_NORMAL are other
    // possible negative values.
    if (desired_priority < THREAD_PRIORITY_NORMAL ||
        desired_priority == kWin7BackgroundThreadModePriority) {
      ret = ::SetThreadPriority(handle, THREAD_PRIORITY_LOWEST);
      // Make sure that using THREAD_PRIORITY_LOWEST didn't affect the memory
      // priority set by THREAD_MODE_BACKGROUND_BEGIN. There is no practical
      // way to verify the I/O priority.
    }
  }

  return ret;
}

bool SetThreadName(std::thread::native_handle_type handle,
                   const std::string& name) {
  // The SetThreadDescription API works even if no debugger is attached.
  static const auto fPointer = reinterpret_cast<PFNSETTHREADDESCRIPTION>(
      reinterpret_cast<void*>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "SetThreadDescription")));
  HRESULT ret = E_NOTIMPL;

  if (handle == 0) {
    handle = ::GetCurrentThread();
  }

  if (fPointer) {
    ret = fPointer(handle, SysUTF8ToWide(name).c_str());
  }

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return SUCCEEDED(ret);

  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadid
#if _WIN32_WINNT < 0x0600
  SetNameInternal(::GetCurrentThreadId(), name.c_str());
#else
  SetNameInternal(::GetThreadId(handle), name.c_str());
#endif
  return SUCCEEDED(ret);
}

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
      static_cast<double>(Frequency.QuadPart);

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
