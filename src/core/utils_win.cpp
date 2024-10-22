// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifdef _WIN32

// We use dynamic loading for below functions
#define GetProductInfo GetProductInfoHidden

struct IUnknown;
#include <windows.h>

#if _WIN32_WINNT > 0x0601
#include <processthreadsapi.h>
#endif
#include <shellapi.h>
#include <shlobj.h>

#include <absl/flags/internal/program_name.h>
#include <base/compiler_specific.h>
#include <build/build_config.h>
#include <limits>

#define MAKE_WIN_VER(major, minor, build_number) (((major) << 24) | ((minor) << 16) | (build_number))

#include "core/logging.hpp"
#include "core/utils.hpp"

// use our dynamic version of GetProductInfo
#undef GetProductInfo

// The most common value returned by ::GetThreadPriority() after background
// thread mode is enabled on Windows 7.
constexpr const int kWin7BackgroundThreadModePriority = 4;

#if 0
// Value sometimes returned by ::GetThreadPriority() after thread priority is
// set to normal on Windows 7.
constexpr const int kWin7NormalPriority = 3;

// These values are sometimes returned by ::GetThreadPriority().
constexpr const int kWinNormalPriority1 = 5;
constexpr const int kWinNormalPriority2 = 6;
#endif

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2022
const DWORD kVCThreadNameException = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

// The SetThreadDescription API was brought in version 1607 of Windows 10.
typedef HRESULT(WINAPI* PFNSETTHREADDESCRIPTION)(HANDLE hThread, PCWSTR lpThreadDescription);

#ifdef COMPILER_MSVC
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
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(DWORD), reinterpret_cast<ULONG_PTR*>(&info));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
}

}  // namespace
#endif  // COMPILER_MSVC

bool SetCurrentThreadPriority(ThreadPriority priority) {
  HANDLE handle = ::GetCurrentThread();
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
    if (desired_priority < THREAD_PRIORITY_NORMAL || desired_priority == kWin7BackgroundThreadModePriority) {
      ret = ::SetThreadPriority(handle, THREAD_PRIORITY_LOWEST);
      // Make sure that using THREAD_PRIORITY_LOWEST didn't affect the memory
      // priority set by THREAD_MODE_BACKGROUND_BEGIN. There is no practical
      // way to verify the I/O priority.
    }
  }

  return ret;
}

bool SetCurrentThreadName(const std::string& name) {
  HANDLE handle = ::GetCurrentThread();
  if (!IsWindowsVersionBNOrGreater(10, 0, 14393)) {
    return true;
  }
  // The SetThreadDescription API works even if no debugger is attached.
  static const auto fPointer = reinterpret_cast<PFNSETTHREADDESCRIPTION>(
      reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"Kernel32.dll"), "SetThreadDescription")));
  HRESULT ret = E_NOTIMPL;

  if (fPointer) {
    ret = fPointer(handle, SysUTF8ToWide(name).c_str());
  }

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return SUCCEEDED(ret);

#ifdef COMPILER_MSVC
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadid
  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadid
  SetNameInternal(::GetCurrentThreadId(), name.c_str());
#endif  // COMPILER_MSVC
  return SUCCEEDED(ret);
}

static uint64_t GetMonotonicTimeQPC() {
  static LARGE_INTEGER StartTime, Frequency;
  static bool started;

  LARGE_INTEGER CurrentTime, ElapsedNanoseconds;

  if (!started) {
    if (!QueryPerformanceFrequency(&Frequency)) {
      RAW_LOG(FATAL, "QueryPerformanceFrequency failed");
      return 0;
    }
    if (!QueryPerformanceCounter(&StartTime)) {
      RAW_LOG(FATAL, "QueryPerformanceCounter failed");
      return 0;
    }
    started = true;
  }
  // Activity to be timed
  if (!QueryPerformanceCounter(&CurrentTime)) {
    RAW_LOG(FATAL, "QueryPerformanceCounter failed");
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

  ElapsedNanoseconds.QuadPart *= NS_PER_SECOND;
  ElapsedNanoseconds.QuadPart /= Frequency.QuadPart;

  return ElapsedNanoseconds.QuadPart;
}

uint64_t GetMonotonicTime() {
#if _WIN32_WINNT >= 0x0600
  return GetMonotonicTimeQPC();
#else
  /* if vista or later */
  static const auto fPointer =
      reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"Kernel32.dll"), "GetTickCount64"));
  if (fPointer) {
    return GetMonotonicTimeQPC();
  }
  return GetTickCount() * 1000000;
#endif
}

static const wchar_t* kDllWhiteList[] = {
#ifdef HAVE_TCMALLOC
#if defined(_M_X64) || defined(_M_ARM64)
    L"tcmalloc.dll",
#else
    L"tcmalloc32.dll",
#endif
#endif  //  HAVE_TCMALLOC
#ifdef HAVE_MIMALLOC
    L"mimalloc-override.dll",
#if defined(_M_X64) || defined(_M_ARM64)
    L"mimalloc-redirect.dll",
#else
    L"mimalloc-redirect32.dll",
#endif
#endif  // HAVE_MIMALLOC
#ifndef _LIBCPP_MSVCRT
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
#endif
    nullptr,
};

static void CheckDynamicLibraries() {
  std::wstring exe(_MAX_PATH, L'\0');
  const auto exeLength = GetModuleFileNameW(nullptr, const_cast<wchar_t*>(exe.c_str()), exe.size() + 1);
  if (!exeLength || exeLength >= exe.size() + 1) {
    RAW_LOG(FATAL, "Could not get executable path!");
  }
  exe.resize(exeLength);
  const auto last1 = exe.find_last_of('\\');
  const auto last2 = exe.find_last_of('/');
  const auto last = std::max((last1 == std::wstring::npos) ? -1 : static_cast<int>(last1),
                             (last2 == std::wstring::npos) ? -1 : static_cast<int>(last2));
  if (last < 0) {
    RAW_LOG(FATAL, "Could not get executable directory!");
  }
  // In the ANSI version of this function,
  // the name is limited to MAX_PATH characters.
  // To extend this limit to approximately 32,000 wide characters,
  // call the Unicode version of the function (FindFirstFileExW),
  // and prepend "\\?\" to the path. For more information, see Naming a File.
  const auto search = L"\\\\?\\" + exe.substr(0, last + 1) + L"*.dll";

  WIN32_FIND_DATAW findData{};

  FINDEX_INFO_LEVELS fInfoLevel = FindExInfoStandard;
  // FindExInfoBasic:
  // This value is not supported until Windows Server 2008 R2 and Windows 7.
  if (IsWindowsVersionBNOrGreater(HIBYTE(_WIN32_WINNT_WIN7), LOBYTE(_WIN32_WINNT_WIN7), 0)) {
    fInfoLevel = FindExInfoBasic;
  }

  HANDLE findHandle = FindFirstFileExW(search.c_str(), fInfoLevel, &findData, FindExSearchNameMatch, nullptr, 0);

  if (findHandle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND) {
      return;
    }
    RAW_LOG(FATAL, "Could not enumerate executable path!");
  }

  do {
    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    const auto me = exe.substr(last + 1);
    if (std::end(kDllWhiteList) !=
        std::find_if(std::begin(kDllWhiteList), std::end(kDllWhiteList),
                     [&findData](const wchar_t* dll) { return dll && _wcsicmp(dll, findData.cFileName) == 0; })) {
      continue;
    }
    std::wostringstream os;
    os << L"\nUnknown DLL library \"" << findData.cFileName << L"\" found in the directory with " << me << ".\n\n"
       << L"This may be a virus or a malicious program. \n\n"
       << L"Please remove all DLL libraries from this directory:\n\n"
       << exe.substr(0, last) << L"\n\n"
       << L"Alternatively, you can move " << me << L" to a new directory.";
    RAW_LOG(FATAL, SysWideToUTF8(os.str()).c_str());
  } while (FindNextFileW(findHandle, &findData));
  FindClose(findHandle);
}

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif  //  LOAD_LIBRARY_SEARCH_SYSTEM32

bool EnableSecureDllLoading() {
  typedef BOOL(WINAPI * SetDefaultDllDirectoriesFunction)(DWORD flags);
  SetDefaultDllDirectoriesFunction set_default_dll_directories = reinterpret_cast<SetDefaultDllDirectoriesFunction>(
      reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetDefaultDllDirectories")));
  if (!set_default_dll_directories) {
    // Don't assert because this is known to be missing on Windows 7 without
    // KB2533623.
    RAW_LOG(WARNING, "SetDefaultDllDirectories unavailable");
    CheckDynamicLibraries();
    return true;
  }

  if (!set_default_dll_directories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
    RAW_LOG(WARNING, "Encountered error calling SetDefaultDllDirectories!");
    CheckDynamicLibraries();
    return true;
  }

  return true;
}

static bool GetProductInfo(DWORD dwOSMajorVersion,
                           DWORD dwOSMinorVersion,
                           DWORD dwSpMajorVersion,
                           DWORD dwSpMinorVersion,
                           PDWORD pdwReturnedProductType) {
  typedef BOOL(WINAPI * GetProductInfoFunction)(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion,
                                                DWORD dwSpMinorVersion, PDWORD pdwReturnedProductType);
  GetProductInfoFunction get_product_info = reinterpret_cast<GetProductInfoFunction>(
      reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "GetProductInfo")));
  if (!get_product_info) {
    *pdwReturnedProductType = 0;
    return false;
  }
  return get_product_info(dwOSMajorVersion, dwOSMinorVersion, dwSpMajorVersion, dwSpMinorVersion,
                          pdwReturnedProductType) != 0;
}

// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfoexa
void GetWindowsVersion(int* major, int* minor, int* build_number, int* os_type) {
  OSVERSIONINFOW version_info{};
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  // GetVersionEx() is deprecated, and the suggested replacement are
  // the IsWindows*OrGreater() functions in VersionHelpers.h. We can't
  // use that because:
  // - For Windows 10, there's IsWindows10OrGreater(), but nothing more
  //   granular. We need to be able to detect different Windows 10 releases
  //   since they sometimes change behavior in ways that matter.
  // - There is no IsWindows11OrGreater() function yet.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (!GetVersionExW(&version_info)) {
    RAW_LOG(FATAL, "Interal error: GetVersionExW failed");
  }
#pragma GCC diagnostic pop

  *major = version_info.dwMajorVersion;
  *minor = version_info.dwMinorVersion;
  *build_number = version_info.dwBuildNumber;
  *os_type = 0;

  if (MAKE_WIN_VER(*major, *minor, *build_number) < MAKE_WIN_VER(6, 0, 0)) {
    return;
  }

  DWORD dwOsType;
  // https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getproductinfo
  if (!GetProductInfo(version_info.dwMajorVersion, version_info.dwMinorVersion, 0, 0, &dwOsType)) {
    RAW_LOG(ERROR, "Interal error: GetProductInfo failed");
  }

  *os_type = dwOsType;
}

bool IsWindowsVersionBNOrGreater(int wMajorVersion, int wMinorVersion, int wBuildNumber) {
  int current_major, current_minor, current_build_number, os_type;
  GetWindowsVersion(&current_major, &current_minor, &current_build_number, &os_type);
  return MAKE_WIN_VER(current_major, current_minor, current_build_number) >=
         MAKE_WIN_VER(wMajorVersion, wMinorVersion, wBuildNumber);
}

static constexpr std::string_view kDefaultExePath = "UNKNOWN";
static std::string main_exe_path = std::string(kDefaultExePath);

bool GetExecutablePath(std::string* exe_path) {
  std::wstring wexe_path;
  exe_path->clear();
  if (!GetExecutablePath(&wexe_path)) {
    return false;
  }
  *exe_path = SysWideToUTF8(wexe_path);

  return true;
}

bool GetExecutablePath(std::wstring* exe_path) {
  DWORD len;
  exe_path->clear();
  // Windows XP:  The string is truncated to nSize characters and is not
  // null-terminated.
  exe_path->resize(_MAX_PATH + 1, L'\0');
  len = ::GetModuleFileNameW(nullptr, const_cast<wchar_t*>(exe_path->data()), _MAX_PATH);
  // If the function succeeds, the return value is the length of the string
  // that is copied to the buffer, in characters,
  // not including the terminating null character.
  exe_path->resize(len);

  // A zero return value indicates a failure other than insufficient space.

  // Insufficient space is determined by a return value equal to the size of
  // the buffer passed in.
  if (len == 0 || len == _MAX_PATH) {
    RAW_LOG(FATAL, "Internal error: GetModuleFileNameW failed");
    *exe_path = SysUTF8ToWide(main_exe_path);
    return false;
  }

  return true;
}

void SetExecutablePath(const std::string& exe_path) {
  main_exe_path = exe_path;

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}

void SetExecutablePath(const std::wstring& exe_path) {
  main_exe_path = SysWideToUTF8(exe_path);

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}

bool GetTempDir(std::string* path) {
  std::wstring wpath;
  path->clear();
  if (!GetTempDir(&wpath)) {
    return false;
  }
  *path = SysWideToUTF8(wpath);
  return true;
}

bool GetTempDir(std::wstring* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPathW(MAX_PATH, temp_path);
  // If the function succeeds, the return value is the length,
  // in TCHARs, of the string copied to lpBuffer,
  // not including the terminating null character.
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  if (temp_path[path_len - 1] == L'\\') {
    temp_path[path_len - 1] = L'\0';
    --path_len;
  }
  *path = std::wstring(temp_path, path_len);
  DCHECK_NE((*path)[path_len - 1], L'\0');
  return true;
}

std::string GetHomeDir() {
  return SysWideToUTF8(GetHomeDirW());
}

std::wstring GetHomeDirW() {
  wchar_t result[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, result)) && result[0]) {
    return result;
  }
  // Fall back to the temporary directory on failure.
  std::wstring rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
  return L"C:\\";
}

ssize_t ReadFileToBuffer(const std::string& path, span<uint8_t> buffer) {
  DCHECK_LE(buffer.size(), std::numeric_limits<DWORD>::max());
  HANDLE hFile =
      ::CreateFileW(SysUTF8ToWide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    return -1;
  }

  DWORD bytes_read;
  if (!::ReadFile(hFile, buffer.data(), buffer.size(), &bytes_read, nullptr)) {
    ::CloseHandle(hFile);
    return -1;
  }

  ::CloseHandle(hFile);

  return bytes_read;
}

ssize_t WriteFileWithBuffer(const std::string& path, std::string_view buf) {
  DWORD written;
  const size_t buf_len = buf.size();
  HANDLE hFile = ::CreateFileW(SysUTF8ToWide(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    DPLOG(WARNING) << "WriteFile failed for path " << path;
    return -1;
  }

  if (!::WriteFile(hFile, buf.data(), buf_len, &written, nullptr)) {
    // WriteFile failed.
    DPLOG(WARNING) << "writing file " << path << " failed";
    ::CloseHandle(hFile);
    return -1;
  }

  if (written != buf_len) {
    // Didn't write all the bytes.
    DLOG(WARNING) << "wrote" << written << " bytes to " << path << " expected " << buf_len;
    ::CloseHandle(hFile);
    return -1;
  }

  ::CloseHandle(hFile);

  return written;
}

PlatformFile OpenReadFile(const std::string& path) {
  return ::CreateFileW(SysUTF8ToWide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
}

PlatformFile OpenReadFile(const std::wstring& path) {
  return ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
}

std::wstring ExpandUserFromString(const std::wstring& path) {
  // the return value is the REQUIRED number of TCHARs,
  // including the terminating NULL character.
  DWORD required_size = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);

  /* if failure or too many bytes required, documented in
   * ExpandEnvironmentStringsW */
  if (required_size == 0 || required_size > 32 * 1024) {
    return path;
  }

  std::wstring expanded_path;
  expanded_path.resize(required_size - 1);
  /* the buffer size should be the string length plus the terminating null character */
  ::ExpandEnvironmentStringsW(path.c_str(), &expanded_path[0], required_size);

  return expanded_path;
}

#endif  // _WIN32
