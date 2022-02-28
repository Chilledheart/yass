// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#include "core/compiler_specific.hpp"
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

static std::string protect_str(DWORD protect) {
  std::string ret;
  switch (protect & 0xf) {
    case PAGE_NOACCESS:
      ret += "noaccess";
      break;
    case PAGE_READONLY:
      ret += "readonly";
      break;
    case PAGE_READWRITE:
      ret += "readwrite";
      break;
    case PAGE_WRITECOPY:
      ret += "writecopy";
      break;
#ifdef PAGE_TARGETS_INVALID
    case PAGE_TARGETS_INVALID:
      ret += "targets-invalid";
      break;
    // case PAGE_TARGETS_NO_UPDATE:
#endif
    default:
      ret += "?";
  }
  if (protect & 0xf0) {
    ret += ",";
    switch (protect & 0xf0) {
      case PAGE_EXECUTE:
        ret += "execute";
        break;
      case PAGE_EXECUTE_READ:
        ret += "execute-read";
        break;
      case PAGE_EXECUTE_READWRITE:
        ret += "execute-readwrite";
        break;
      case PAGE_EXECUTE_WRITECOPY:
        ret += "execute-writecopy";
        break;
      default:
        ret += "execute-?";
    }
  }
  if (protect & 0xf00) {
    ret += ",";
    switch (protect & 0xf00) {
      case PAGE_GUARD:
        ret += "guard";
        break;
      case PAGE_NOCACHE:
        ret += "nocache";
        break;
      case PAGE_WRITECOMBINE:
        ret += "write-combine";
        break;
      default:
        ret += "protect-?";
    }
  }
  return ret;
}

static const char* state_str(DWORD state) {
  switch (state) {
    case 0:
      return "none";
    case MEM_COMMIT:
      return "commit";
    case MEM_FREE:
      return "free";
    case MEM_RESERVE:
      return "reserve";
    default:
      return "?";
  }
}


static const char* type_str(DWORD type) {
  switch (type) {
    case 0:
      return "none";
    case MEM_IMAGE:
      return "image";
    case MEM_MAPPED:
      return "mapped";
    case MEM_PRIVATE:
      return "private";
    default:
      return "?";
  }
}

bool MemoryLockAll() {
#ifdef ARCH_CPU_64_BITS
  DWORD size = 15 * 1024 * 1024; /* 15MB */
#else
  DWORD size = 5 * 1024 * 1024; /* 5MB */
#endif
  HANDLE process = ::GetCurrentProcess();
  if (!::SetProcessWorkingSetSize(process, size, size)) {
    PLOG(WARNING) << "Failed to set working set";
    return false;
  }
  MEMORY_BASIC_INFORMATION memInfo {};
  uintptr_t *address = nullptr;
  bool failed = false;
  while (::VirtualQueryEx(process, address, &memInfo, sizeof(memInfo))) {
    bool lockable = memInfo.State == MEM_COMMIT &&
      !(memInfo.Protect & PAGE_NOACCESS) &&
      !(memInfo.Protect & PAGE_GUARD);

    VLOG(4) << "Calling VirtualLock on address: " << memInfo.BaseAddress
            << " AllocationBase: " << memInfo.AllocationBase
            << " AllocationProtect: " << protect_str(memInfo.AllocationProtect)
#ifdef _WIN64
            << " PartitionId: " << memInfo.PartitionId
#endif
            << " RegionSize: " << memInfo.RegionSize
            << " State: " << state_str(memInfo.State)
            << " Protect: " << protect_str(memInfo.Protect)
            << " Type: " << type_str(memInfo.Type);

    if (lockable && !::VirtualLock(memInfo.BaseAddress, memInfo.RegionSize)) {
      PLOG(WARNING) << "Failed to call VirtualLock on address: "
                    << memInfo.BaseAddress
                    << " State:" << state_str(memInfo.State)
                    << " Protect: " << protect_str(memInfo.Protect)
                    << " Type: " << type_str(memInfo.Type);
      failed = true;
    }
    // Move to the next region
    address += memInfo.RegionSize;
  }
  return !failed;
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

static bool IsHandleConsole(HANDLE handle) {
  DWORD mode;
  return handle != 0 && handle != INVALID_HANDLE_VALUE &&
         (GetFileType(handle) & ~FILE_TYPE_REMOTE) == FILE_TYPE_CHAR &&
         GetConsoleMode(handle, &mode);
}

bool IsProgramConsole() {
  return IsHandleConsole(GetStdHandle(STD_INPUT_HANDLE)) ||
    IsHandleConsole(GetStdHandle(STD_OUTPUT_HANDLE)) ||
    IsHandleConsole(GetStdHandle(STD_ERROR_HANDLE));
}

static void GetWindowsVersion(int *major, int *minor, int *build_number,
                              DWORD *os_type) {
  OSVERSIONINFOW version_info {};
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  // GetVersionEx() is deprecated, and the suggested replacement are
  // the IsWindows*OrGreater() functions in VersionHelpers.h. We can't
  // use that because:
  // - For Windows 10, there's IsWindows10OrGreater(), but nothing more
  //   granular. We need to be able to detect different Windows 10 releases
  //   since they sometimes change behavior in ways that matter.
  // - There is no IsWindows11OrGreater() function yet.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (!GetVersionExW(&version_info)) {
    PLOG(WARNING) << "Interal error: GetVersionExW failed";
  }
#pragma clang diagnostic pop

  *major = version_info.dwMajorVersion;
  *minor = version_info.dwMinorVersion;
  *build_number = version_info.dwBuildNumber;
  *os_type = 0;

  // FIXME use dynamic load on Kernel32.dll
  // https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
#if _WIN32_WINNT >= 0x0601
  if (!GetProductInfo(version_info.dwMajorVersion, version_info.dwMinorVersion,
                      0, 0, os_type)) {
    PLOG(WARNING) << "Interal error: GetProductInfo failed";
  }
#endif
}

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif // CP_UTF8

#define MAKE_VER(major, minor, build) \
    (((major) << 24) | ((minor) << 16) | (build))

bool SetUTF8Locale() {
  bool success = false;

  int major, minor, build_number;
  DWORD os_type;
  uint32_t version;
  DWORD processes[2];

  GetWindowsVersion(&major, &minor, &build_number, &os_type);

  bool is_console = IsProgramConsole();

  if (is_console) {
    // Calling SetConsoleCP
    // this setting is permanent and we need to restore CP value after exit,
    // otherwise it might affects the all programs in the same console
    //
    // anyway using WriteConsoleW should be sufficient to handle with UTF-8
    // characters in console, better to leave this code path aside.
#if 0
    // Use UTF-8 mode on Windows 10 1903 or later.
    if (MAKE_VER(major, minor, build_number) >= MAKE_VER(10, 0, 18362)) {
      static UINT previous_cp, previous_output_cp;
      previous_cp = GetConsoleCP();
      previous_output_cp = GetConsoleOutputCP();
      if (SetConsoleCP(CP_UTF8) && SetConsoleOutputCP(CP_UTF8)) {
        success = true;
      } else {
        PLOG(WARNING) << "Interal error: setup utf-8 console cp failed";
      }
      std::atexit([]() {
        if (!SetConsoleCP(previous_cp) ||
            !SetConsoleOutputCP(previous_output_cp)) {
          PLOG(WARNING) << "Interal error: restore console cp failed";
        }
      });
    }
#else
    success = true;
#endif
  } else {
    success = true;
  }

  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170
  if (MAKE_VER(major, minor, build_number) >= MAKE_VER(10, 0, 17134)) {
    setlocale(LC_ALL, ".UTF8");
  } else if (!is_console) {
    success = false;
  }
  return success;
}

#undef MAKE_VER

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

static const wchar_t *kDllWhiteList [] = {
// msvc runtime, still searched current directory
// under dll search security mode
  L"MSVCP140.dll",
  L"msvcp140_1.dll",
  L"msvcp140_2.dll",
  L"msvcp140_atomic_wait.dll",
  L"msvcp140_codecvt_ids.dll",
  L"VCRUNTIME140.dll",
  L"VCRUNTIME140_1.dll",
  L"CONCRT140.dll",
// ucrt
  L"api-ms-win-core-console-l1-1-0.dll",
  L"api-ms-win-core-datetime-l1-1-0.dll",
  L"api-ms-win-core-debug-l1-1-0.dll",
  L"api-ms-win-core-errorhandling-l1-1-0.dll",
  L"api-ms-win-core-file-l1-1-0.dll",
  L"api-ms-win-core-file-l1-2-0.dll",
  L"api-ms-win-core-file-l2-1-0.dll",
  L"api-ms-win-core-handle-l1-1-0.dll",
  L"api-ms-win-core-heap-l1-1-0.dll",
  L"api-ms-win-core-interlocked-l1-1-0.dll",
  L"api-ms-win-core-libraryloader-l1-1-0.dll",
  L"api-ms-win-core-localization-l1-2-0.dll",
  L"api-ms-win-core-memory-l1-1-0.dll",
  L"api-ms-win-core-namedpipe-l1-1-0.dll",
  L"api-ms-win-core-processenvironment-l1-1-0.dll",
  L"api-ms-win-core-processthreads-l1-1-0.dll",
  L"api-ms-win-core-processthreads-l1-1-1.dll",
  L"api-ms-win-core-profile-l1-1-0.dll",
  L"api-ms-win-core-rtlsupport-l1-1-0.dll",
  L"api-ms-win-core-string-l1-1-0.dll",
  L"api-ms-win-core-synch-l1-1-0.dll",
  L"api-ms-win-core-synch-l1-2-0.dll",
  L"api-ms-win-core-sysinfo-l1-1-0.dll",
  L"api-ms-win-core-timezone-l1-1-0.dll",
  L"api-ms-win-core-util-l1-1-0.dll",
  L"api-ms-win-crt-conio-l1-1-0.dll",
  L"api-ms-win-crt-convert-l1-1-0.dll",
  L"api-ms-win-crt-environment-l1-1-0.dll",
  L"api-ms-win-crt-filesystem-l1-1-0.dll",
  L"api-ms-win-crt-heap-l1-1-0.dll",
  L"api-ms-win-crt-locale-l1-1-0.dll",
  L"api-ms-win-crt-math-l1-1-0.dll",
  L"api-ms-win-crt-multibyte-l1-1-0.dll",
  L"api-ms-win-crt-private-l1-1-0.dll",
  L"api-ms-win-crt-process-l1-1-0.dll",
  L"api-ms-win-crt-runtime-l1-1-0.dll",
  L"api-ms-win-crt-stdio-l1-1-0.dll",
  L"api-ms-win-crt-string-l1-1-0.dll",
  L"api-ms-win-crt-time-l1-1-0.dll",
  L"api-ms-win-crt-utility-l1-1-0.dll",
  L"ucrtbase.dll",
};

static void CheckDynamicLibraries() {
  std::wstring exe(MAX_PATH, L'\0');
  const auto exeLength = GetModuleFileNameW(
    nullptr,
    const_cast<wchar_t*>(exe.c_str()),
    exe.size() + 1);
  if (!exeLength || exeLength >= exe.size() + 1) {
    PLOG(FATAL) << L"Could not get executable path!";
  }
  exe.resize(exeLength);
  const auto last1 = exe.find_last_of('\\');
  const auto last2 = exe.find_last_of('/');
  const auto last = std::max(
    (last1 == std::wstring::npos) ? -1 : int(last1),
    (last2 == std::wstring::npos) ? -1 : int(last2));
  if (last < 0) {
    LOG(FATAL) << L"Could not get executable directory!";
  }
  // In the ANSI version of this function,
  // the name is limited to MAX_PATH characters.
  // To extend this limit to approximately 32,000 wide characters,
  // call the Unicode version of the function (FindFirstFileExW),
  // and prepend "\\?\" to the path. For more information, see Naming a File.
  const auto search = L"\\\\?\\" + exe.substr(0, last + 1) + L"*.dll";

  WIN32_FIND_DATAW findData{};
#if _WIN32_WINNT >= 0x0601
  // FindExInfoBasic:
  // This value is not supported until Windows Server 2008 R2 and Windows 7.
  HANDLE findHandle = FindFirstFileExW(search.c_str(), FindExInfoBasic,
                                       &findData, FindExSearchNameMatch,
                                       nullptr, 0);
#else
  HANDLE findHandle = FindFirstFileExW(search.c_str(), FindExInfoStandard,
                                       &findData, FindExSearchNameMatch,
                                       nullptr, 0);
#endif
  if (findHandle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND) {
      return;
    }
    PLOG(FATAL) << L"Could not enumerate executable path!";
  }

  do {
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    const auto me = exe.substr(last + 1);
    if (std::end(kDllWhiteList) != std::find_if(
      std::begin(kDllWhiteList), std::end(kDllWhiteList),
      [&findData](const wchar_t* dll) {
        return wcsicmp(dll, findData.cFileName) == 0;
      })) {
      continue;
    }
    std::wstringstream os;
    os << L"\nUnknown DLL library \""<< findData.cFileName
      << L"\" found in the directory with " << me << L".\n\n"
      << L"This may be a virus or a malicious program. \n\n"
      << L"Please remove all DLL libraries from this directory:\n\n"
      << exe.substr(0, last) << L"\n\n"
      << "Alternatively, you can move " << me << L" to a new directory.";
    LOG(FATAL) << SysWideToUTF8(os.str());
  } while (FindNextFileW(findHandle, &findData));
  FindClose(findHandle);
}

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif //  LOAD_LIBRARY_SEARCH_SYSTEM32

bool EnableSecureDllLoading() {
  typedef BOOL(WINAPI * SetDefaultDllDirectoriesFunction)(DWORD flags);
  SetDefaultDllDirectoriesFunction set_default_dll_directories =
      reinterpret_cast<SetDefaultDllDirectoriesFunction>(::GetProcAddress(
          ::GetModuleHandleW(L"kernel32.dll"), "SetDefaultDllDirectories"));
  if (!set_default_dll_directories) {
    // Don't assert because this is known to be missing on Windows 7 without
    // KB2533623.
    PLOG(WARNING) << "SetDefaultDllDirectories unavailable";
    CheckDynamicLibraries();
    return true;
  }

  if (!set_default_dll_directories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
    PLOG(WARNING) << "Encountered error calling SetDefaultDllDirectories!";
    CheckDynamicLibraries();
    return true;
  }

  return true;
}

#endif  // _WIN32
