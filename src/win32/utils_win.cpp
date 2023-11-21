// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2023 Chilledheart  */

// We use dynamic loading for below functions
#define GetDeviceCaps GetDeviceCapsHidden
#define SetProcessDPIAware SetProcessDPIAwareHidden

#include "win32/utils.hpp"

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "config/config.hpp"

#include <sstream>

#ifdef _WIN32

#define DEFAULT_AUTOSTART_KEY \
  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
static constexpr size_t kRegReadMaximumSize = 1024 * 1024;

#ifdef COMPILER_MSVC
#include <Tchar.h>
#else
#include <tchar.h>
#endif  // COMPILER_MSVC
#include <windows.h>
#include <wininet.h>
#include <ras.h>
#include <raserror.h>

// https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
//
// Windows 10 Unknown                             NTDDI_WIN10_CO (0x0A00000B)
// Windows 10 Unknown                             NTDDI_WIN10_FE (0x0A00000A)
// Windows 10 Unknown                             NTDDI_WIN10_MN (0x0A000009)
// Windows 10 2004                                NTDDI_WIN10_VB (0x0A000008)
// Windows 10 1903 "19H1"                         NTDDI_WIN10_19H1 (0x0A000007)
// Windows 10 1809 "Redstone 5"                   NTDDI_WIN10_RS5 (0x0A000006)
// Windows 10 1803 "Redstone 4"                   NTDDI_WIN10_RS4 (0x0A000005)
// Windows 10 1709 "Redstone 3"                   NTDDI_WIN10_RS3 (0x0A000004)
// Windows 10 1703 "Redstone 2"                   NTDDI_WIN10_RS2 (0x0A000003)
// Windows 10 1607 "Redstone 1"                   NTDDI_WIN10_RS1 (0x0A000002)
// Windows 10 1511 "Threshold 2"                  NTDDI_WIN10_TH2 (0x0A000001)
// Windows 10 1507 "Threshold"                    NTDDI_WIN10 (0x0A000000)
// Windows 8.1                                    NTDDI_WINBLUE (0x06030000)
// Windows 8                                      NTDDI_WIN8 (0x06020000)
// Windows 7                                      NTDDI_WIN7 (0x06010000)
// Windows Server 2008                            NTDDI_WS08 (0x06000100)
// Windows Vista with Service Pack 1 (SP1)        NTDDI_VISTASP1 (0x06000100)
// Windows Vista                                  NTDDI_VISTA (0x06000000)
// Windows Server 2003 with Service Pack 2 (SP2)  NTDDI_WS03SP2 (0x05020200)
// Windows Server 2003 with Service Pack 1 (SP1)  NTDDI_WS03SP1 (0x05020100)
// Windows Server 2003                            NTDDI_WS03 (0x05020000)
// Windows XP with Service Pack 3 (SP3)           NTDDI_WINXPSP3 (0x05010300)
// Windows XP with Service Pack 2 (SP2)           NTDDI_WINXPSP2 (0x05010200)
// Windows XP with Service Pack 1 (SP1)           NTDDI_WINXPSP1 (0x05010100)
// Windows XP                                     NTDDI_WINXP (0x05010000)
#ifdef COMPILER_MSVC
#include <SDKDDKVer.h>
#else
#include <sdkddkver.h>
#endif  //  COMPILER_MSVC

// from wingdi.h, starting from Windows 2000
typedef int(__stdcall* PFNGETDEVICECAPS)(HDC, int);

// from Winuser.h, starting from Windows Vista
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif  // USER_DEFAULT_SCREEN_DPI

// from winuser.h, starting from Windows Vista
typedef BOOL(__stdcall* PFNSETPROCESSDPIAWARE)(void);

// from shellscalingapi.h, starting from Windows 8.1
typedef enum PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
  MDT_EFFECTIVE_DPI = 0,
  MDT_ANGULAR_DPI = 1,
  MDT_RAW_DPI = 2,
  MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

// from shellscalingapi.h, starting from Windows 8.1
typedef HRESULT(__stdcall* PFNSETPROCESSDPIAWARENESS)(PROCESS_DPI_AWARENESS);
// from shellscalingapi.h, starting from Windows 8.1
typedef HRESULT(__stdcall* PFNGETDPIFORMONITOR)(HMONITOR,
                                                MONITOR_DPI_TYPE,
                                                UINT*,
                                                UINT*);

// from windef.h, starting from Windows 10, version 1607
#if !defined(NTDDI_WIN10_RS1) || (defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 8)
typedef enum DPI_AWARENESS {
  DPI_AWARENESS_INVALID = -1,
  DPI_AWARENESS_UNAWARE = 0,
  DPI_AWARENESS_SYSTEM_AWARE = 1,
  DPI_AWARENESS_PER_MONITOR_AWARE = 2
} DPI_AWARENESS;
#endif  // NTDDI_WIN10_RS1

// from windef.h, starting from Windows 10, version 1607
#if !defined(NTDDI_WIN10_RS1) || (defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 8)
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#define DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED ((DPI_AWARENESS_CONTEXT)-5)
#endif  // NTDDI_WIN10_RS1

// from windef.h, starting from Windows 10, version 1803
#if !defined(NTDDI_WIN10_RS4) || (defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 8)
typedef enum DPI_HOSTING_BEHAVIOR {
  DPI_HOSTING_BEHAVIOR_INVALID = -1,
  DPI_HOSTING_BEHAVIOR_DEFAULT = 0,
  DPI_HOSTING_BEHAVIOR_MIXED = 1
} DPI_HOSTING_BEHAVIOR;
#endif  // NTDDI_WIN10_RS4

// from winuser.h, starting from Windows 10, version 1607
typedef DPI_AWARENESS_CONTEXT(__stdcall* PFNGETTHREADDPIAWARENESSCONTEXT)(void);
// from winuser.h, starting from Windows 10, version 1607
typedef DPI_AWARENESS_CONTEXT(__stdcall* PFNGETWINDOWDPIAWARENESSCONTEXT)(HWND);
// from winuser.h, starting from Windows 10, version 1607
typedef DPI_AWARENESS(__stdcall* PFNGETAWARENESSFROMDPIAWARENESSCONTEXT)(
    DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1703
typedef BOOL(__stdcall* PFNSETPROCESSDPIAWARENESSCONTEXT)(
    DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1607
typedef BOOL(__stdcall* PFNSETTHREADDPIAWARENESSCONTEXT)(DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1607
typedef BOOL(__stdcall* PFNISVALIDDPIAWARENESScONTEXT)(DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1607
typedef BOOL(__stdcall* PFNAREDPIAWARENESSCONTEXTSEQUAL)(DPI_AWARENESS_CONTEXT,
                                                         DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1607
typedef UINT(__stdcall* PFNGETDPIFORSYSTEM)(void);
// from winuser.h, starting from Windows 10, version 1607
typedef UINT(__stdcall* PFNGETDPIFORWINDOW)(HWND);
// from winuser.h, starting from Windows 10, version 1803
typedef UINT(__stdcall* PFNGETDPIFROMDPIAWARENESSCONTEXT)(
    DPI_AWARENESS_CONTEXT);
// from winuser.h, starting from Windows 10, version 1803
typedef DPI_HOSTING_BEHAVIOR(__stdcall* PFNSETTHREADDPIHOSTINGBEHAVIOR)(
    DPI_HOSTING_BEHAVIOR);
// from winuser.h, starting from Windows 10, version 1607
typedef BOOL(__stdcall* PFNENABLENONCLIENTDPISCALING)(HWND);
// from winuser.h, starting from Windows 10, version 1607
typedef BOOL(__stdcall* PFNSYSTEMPARAMETERSINFOFORDPI)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);

// from winnls.h, starting from Windows Vista
typedef int(__stdcall* PFNGETUSERDEFAULTLOCALENAME)(LPWSTR lpLocaleName, int cchLocaleName);

// We use dynamic loading for below functions
#undef GetDeviceCaps
#undef SetProcessDPIAware

namespace {

HANDLE EnsureUser32Loaded() {
  return LoadLibraryExW(L"User32.dll", nullptr, 0);
}

HANDLE EnsureGdi32Loaded() {
  return LoadLibraryExW(L"Gdi32.dll", nullptr, 0);
}

HANDLE EnsureShcoreLoaded() {
  return LoadLibraryExW(L"Shcore.dll", nullptr, 0);
}

HANDLE EnsureKernel32Loaded() {
  return LoadLibraryExW(L"Kernel32.dll", nullptr, 0);
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getdevicecaps
int GetDeviceCaps(HDC hdc, int index) {
  static const auto fPointer = reinterpret_cast<PFNGETDEVICECAPS>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureGdi32Loaded()), "GetDeviceCaps")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return USER_DEFAULT_SCREEN_DPI;
  }

  return fPointer(hdc, index);
}

// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setprocessdpiaware
BOOL SetProcessDPIAware() {
  static const auto fPointer = reinterpret_cast<PFNSETPROCESSDPIAWARE>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureUser32Loaded()), "SetProcessDPIAware")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }

  return fPointer();
}

HRESULT SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value) {
  static const auto fPointer =
      reinterpret_cast<PFNSETPROCESSDPIAWARENESS>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureShcoreLoaded()),
                           "SetProcessDpiAwareness")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return E_NOTIMPL;
  }

  return fPointer(value);
}

HRESULT GetDpiForMonitor(HMONITOR hmonitor,
                         MONITOR_DPI_TYPE dpiType,
                         UINT* dpiX,
                         UINT* dpiY) {
  static const auto fPointer = reinterpret_cast<PFNGETDPIFORMONITOR>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureShcoreLoaded()), "GetDpiForMonitor")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return E_NOTIMPL;
  }

  return fPointer(hmonitor, dpiType, dpiX, dpiY);
}

DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext() {
  static const auto fPointer =
      reinterpret_cast<PFNGETTHREADDPIAWARENESSCONTEXT>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "GetThreadDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return DPI_AWARENESS_CONTEXT_UNAWARE;
  }
  return fPointer();
}

DPI_AWARENESS_CONTEXT GetWindowDpiAwarenessContext(HWND hwnd) {
  static const auto fPointer =
      reinterpret_cast<PFNGETWINDOWDPIAWARENESSCONTEXT>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "GetWindowDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return DPI_AWARENESS_CONTEXT_UNAWARE;
  }
  return fPointer(hwnd);
}

DPI_AWARENESS GetAwarenessFromDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  static const auto fPointer =
      reinterpret_cast<PFNGETAWARENESSFROMDPIAWARENESSCONTEXT>(
          reinterpret_cast<void*>(
              ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                               "GetAwarenessFromDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return DPI_AWARENESS_INVALID;
  }
  return fPointer(value);
}

BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  static const auto fPointer =
      reinterpret_cast<PFNSETPROCESSDPIAWARENESSCONTEXT>(
          reinterpret_cast<void*>(
              ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                               "SetProcessDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(value);
}

BOOL SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  static const auto fPointer =
      reinterpret_cast<PFNSETTHREADDPIAWARENESSCONTEXT>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "SetThreadDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(value);
}

// Determines if a specified DPI_AWARENESS_CONTEXT is valid and supported
// by the current system.
BOOL IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  static const auto fPointer =
      reinterpret_cast<PFNISVALIDDPIAWARENESScONTEXT>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "IsValidDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(value);
}

// Determines if a specified DPI_AWARENESS_CONTEXT is valid and supported
// by the current system.
BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT dpiContextA,
                                  DPI_AWARENESS_CONTEXT dpiContextB) {
  static const auto fPointer =
      reinterpret_cast<PFNAREDPIAWARENESSCONTEXTSEQUAL>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "AreDpiAwarenessContextsEqual")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(dpiContextA, dpiContextB);
}

// The return value will be dependent based upon the calling context.
// If the current thread has a DPI_AWARENESS value of DPI_AWARENESS_UNAWARE,
// the return value will be 96. That is because the current context always
// assumes a DPI of 96. For any other DPI_AWARENESS value,
// the return value will be the actual system DPI.
UINT GetDpiForSystem() {
  static const auto fPointer = reinterpret_cast<PFNGETDPIFORSYSTEM>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureUser32Loaded()), "GetDpiForSystem")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
  }
  return fPointer();
}

// clang-format off
// The DPI for the window which depends on the DPI_AWARENESS of the window.
// An invalid hwnd value will result in a return value of 0.
// DPI_AWARENESS                   Return value
// DPI_AWARENESS_UNAWARE           USER_DEFAULT_SCREEN_DPI
// DPI_AWARENESS_SYSTEM_AWARE      The system DPI.
// DPI_AWARENESS_PER_MONITOR_AWARE The DPI of the monitor where the window is located.
// clang-format on
UINT GetDpiForWindow(HWND hwnd) {
  static const auto fPointer = reinterpret_cast<PFNGETDPIFORWINDOW>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureUser32Loaded()), "GetDpiForWindow")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
  }
  return fPointer(hwnd);
}

// DPI_AWARENESS_CONTEXT handles associated with values of
// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE and
// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 will return a value of 0 for
// their DPI.
UINT GetDpiFromDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
  static const auto fPointer =
      reinterpret_cast<PFNGETDPIFROMDPIAWARENESSCONTEXT>(
          reinterpret_cast<void*>(
              ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                               "GetDpiFromDpiAwarenessContext")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
  }
  return fPointer(value);
}

// Sets the thread's DPI_HOSTING_BEHAVIOR. This behavior allows windows created
// in the thread to host child windows with a different DPI_AWARENESS_CONTEXT.
DPI_HOSTING_BEHAVIOR SetThreadDpiHostingBehavior(DPI_HOSTING_BEHAVIOR value) {
  static const auto fPointer =
      reinterpret_cast<PFNSETTHREADDPIHOSTINGBEHAVIOR>(reinterpret_cast<void*>(
          ::GetProcAddress(static_cast<HMODULE>(EnsureUser32Loaded()),
                           "SetThreadDpiHostingBehavior")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return DPI_HOSTING_BEHAVIOR_INVALID;
  }
  return fPointer(value);
}

// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablenonclientdpiscaling
//
// In high-DPI displays, enables automatic display scaling of the non-client
// area portions of the specified top-level window.
// Must be called during the initialization of that window.
BOOL EnableNonClientDpiScaling(HWND hwnd) {
  static const auto fPointer = reinterpret_cast<PFNENABLENONCLIENTDPISCALING>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureUser32Loaded()),
          "EnableNonClientDpiScaling")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(hwnd);
}

// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfofordpi
// Retrieves the value of one of the system-wide parameters, taking into account the provided DPI value.
BOOL SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) {
  static const auto fPointer = reinterpret_cast<PFNSYSTEMPARAMETERSINFOFORDPI>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureUser32Loaded()),
          "SystemParametersInfoForDpi")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
  }
  return fPointer(uiAction, uiParam, pvParam, fWinIni, dpi);
}

// https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getuserdefaultlocalename?redirectedfrom=MSDN
// Retrieves the user default locale name.
#if _WIN32_WINNT < 0x0600
int GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName) {
  static const auto fPointer = reinterpret_cast<PFNGETUSERDEFAULTLOCALENAME>(
      reinterpret_cast<void*>(::GetProcAddress(
          static_cast<HMODULE>(EnsureKernel32Loaded()),
          "GetUserDefaultLocaleName")));
  if (fPointer == nullptr) {
    ::SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
  }
  return fPointer(lpLocaleName, cchLocaleName);
}
#endif

class ScopedHKEY {
 public:
   explicit ScopedHKEY(HKEY hkey) : hkey_(hkey) {}
   ~ScopedHKEY() {
     if (hkey_) {
       ::RegCloseKey(hkey_);
     }
   }

 private:
   HKEY hkey_;
};

}  // namespace

static bool OpenKey(HKEY *hkey, bool isWrite) {
  DWORD disposition;
  const wchar_t* subkey = DEFAULT_AUTOSTART_KEY;
  REGSAM samDesired = (isWrite ? KEY_SET_VALUE : KEY_QUERY_VALUE);

  // Creates the specified registry key. If the key already exists, the
  // function opens it. Note that key names are not case sensitive.
  if (::RegCreateKeyExW(
      HKEY_CURRENT_USER /* HKEY */, subkey /* lpSubKey */, 0 /* Reserved */,
      nullptr /*lpClass*/, REG_OPTION_NON_VOLATILE /* dwOptions */,
      samDesired /* samDesired */, nullptr /* lpSecurityAttributes */,
      hkey /* phkResult */,
      &disposition /* lpdwDisposition */) == ERROR_SUCCESS) {
    if (disposition == REG_CREATED_NEW_KEY) {
    } else if (disposition == REG_OPENED_EXISTING_KEY) {
    }
    return true;
  }
  return false;
}

static bool OpenReadableKey(HKEY *hkey) {
  return OpenKey(hkey, false);
}

static bool OpenWritableKey(HKEY *hkey) {
  return OpenKey(hkey, true);
}


// https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
// https://docs.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process
// https://docs.microsoft.com/en-us/windows/win32/hidpi/dpi-awareness-context
// https://docs.microsoft.com/en-us/windows/win32/api/shellscalingapi/nf-shellscalingapi-getscalefactorformonitor
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setprocessdpiawarenesscontext
//
// many Win32 APIs do not have any DPI or display context so they will only
// return values relative to the System DPI. It can be useful to grep through
// your code to look for some of these APIs and replace them with DPI-aware
// variants. Some of the common APIs that have DPI-aware variants are:
//
// Single DPI version    Per-Monitor version
// GetSystemMetrics      GetSystemMetricsForDpi
// AdjustWindowRectEx    AdjustWindowRectExForDpi
// SystemParametersInfo  SystemParametersInfoForDpi
// GetDpiForMonitor      GetDpiForWindow
//
// https://mariusbancila.ro/blog/2021/05/19/how-to-build-high-dpi-aware-native-desktop-applications/
//
// Part of the solution is to replace Win32 APIs that only support a single DPI
// (the primary display DPI) with ones that support per-monitor settings, where
// available, or write your own, where that’s not available.
//
// Here is a list of such APIs:
// System (primary monitor) DPI    Per-monitor DPI
// GetDeviceCaps                   GetDpiForMonitor / GetDpiForWindow
// GetSystemMetrics                GetSystemMetricsForDpi
// SystemParametersInfo            SystemParametersInfoForDpi
// AdjustWindowRectEx              AdjustWindowRectExForDpi
// CWnd::CalcWindowRect            AdjustWindowRectExForDpi
//
// clang-format off
// API                           Minimum version of Windows DPI Unaware                   System DPI Aware                    Per Monitor DPI Aware
// SetProcessDPIAware            Windows Vista              N/A                           SetProcessDPIAware                  N/A
// SetProcessDpiAwareness        Windows 8.1                PROCESS_DPI_UNAWARE           PROCESS_SYSTEM_DPI_AWARE            PROCESS_PER_MONITOR_DPI_AWARE
// SetProcessDpiAwarenessContext Windows 10, version 1607   DPI_AWARENESS_CONTEXT_UNAWARE DPI_AWARENESS_CONTEXT_SYSTEM_AWARE  DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE/V2
// clang-format on
bool Utils::SetDpiAwareness(DpiAwarenessType awareness_type) {
  DPI_AWARENESS_CONTEXT awareness_context{DPI_AWARENESS_CONTEXT_UNAWARE};
  switch (awareness_type) {
    case DpiAwarenessPerMonitorV2:
      awareness_context = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
      break;
    case DpiAwarenessPerMonitor:
      awareness_context = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
      break;
    case DpiAwarenessSystem:
      awareness_context = DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
      break;
    case DpiAwarenessUnware:
      awareness_context = DPI_AWARENESS_CONTEXT_UNAWARE;
      break;
    default:
      awareness_context = DPI_AWARENESS_CONTEXT_UNAWARE;
      break;
  };

  if (IsValidDpiAwarenessContext(awareness_context)) {
    if (SetThreadDpiAwarenessContext(awareness_context)) {
      VLOG(2) << "Win10 style's ThreadDpiAwareness is set up";
      return true;
    }
    VLOG(2) << "ThreadDpiAwareness is not set, falling back...";

    if (SetProcessDpiAwarenessContext(awareness_context)) {
      VLOG(2) << "Win10 style's ProcessDpiAwareness (all threads) is set up";
      return true;
    }
  }
  VLOG(2) << "ProcessDpiAwareness is not set, falling back...";

  PROCESS_DPI_AWARENESS dpi_awareness = PROCESS_DPI_UNAWARE;
  switch (awareness_type) {
    case DpiAwarenessPerMonitorV2:
      dpi_awareness = PROCESS_PER_MONITOR_DPI_AWARE;
      break;
    case DpiAwarenessPerMonitor:
      dpi_awareness = PROCESS_PER_MONITOR_DPI_AWARE;
      break;
    case DpiAwarenessSystem:
      dpi_awareness = PROCESS_SYSTEM_DPI_AWARE;
      break;
    case DpiAwarenessUnware:
      dpi_awareness = PROCESS_DPI_UNAWARE;
      break;
    default:
      dpi_awareness = PROCESS_DPI_UNAWARE;
      break;
  };

  HRESULT hr = SetProcessDpiAwareness(dpi_awareness);
  if (SUCCEEDED(hr)) {
    VLOG(2) << "Win8.1 style's ProcessDpiAwareness (all threads) is set up";
    return true;
  }

  VLOG(2) << "SetProcessDpiAwareness failed, falling back...";

  if (SetProcessDPIAware()) {
    VLOG(2) << "Vista style's ProcessDPIAware is set up";
    return true;
  }

  VLOG(2) << "all SetDpiAwareness methods tried, no support for HiDpi";

  return false;
}

bool Utils::SetMixedThreadDpiHostingBehavior() {
  if (SetThreadDpiHostingBehavior(DPI_HOSTING_BEHAVIOR_MIXED) ==
      DPI_HOSTING_BEHAVIOR_INVALID) {
    VLOG(2) << "Mixed DPI hosting behavior not applied.";
    return false;
  }
  VLOG(2) << "Mixed DPI hosting behavior applied.";
  return true;
}

// https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DPIAwarenessPerWindow/client/DpiAwarenessContext.cpp
unsigned int Utils::GetDpiForWindowOrSystem(HWND hWnd) {
  // Get the DPI awareness of the window
  DPI_AWARENESS_CONTEXT awareness_context = GetWindowDpiAwarenessContext(hWnd);
  if (!IsValidDpiAwarenessContext(awareness_context)) {
    // Get the DPI awareness of the thread
    VLOG(2) << "Window's DpiAwareness Context is not found, falling back...";
    awareness_context = GetThreadDpiAwarenessContext();
  }

  if (IsValidDpiAwarenessContext(awareness_context)) {
    VLOG(2) << "Thread's DpiAwareness Context is found, setting up...";

    if (UINT uDpi = GetDpiFromDpiAwarenessContext(awareness_context)) {
      VLOG(2) << "DPI: Use Dpi in Awareness Context";
      return uDpi;
    }

    if (AreDpiAwarenessContextsEqual(awareness_context,
                                     DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED)) {
      VLOG(2) << "DPI Awareness: Unware GPIScaled found";
    } else if (AreDpiAwarenessContextsEqual(
                   awareness_context,
                   DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
      VLOG(2) << "DPI Awareness: Per Monitor Aware v2 found";
    } else if (AreDpiAwarenessContextsEqual(
                   awareness_context,
                   DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
      VLOG(2) << "DPI Awareness: Per Monitor Aware found";
    } else if (AreDpiAwarenessContextsEqual(
                   awareness_context, DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) {
      VLOG(2) << "DPI Awareness: System Aware found";
    } else if (AreDpiAwarenessContextsEqual(awareness_context,
                                            DPI_AWARENESS_CONTEXT_UNAWARE)) {
      VLOG(2) << "DPI Awareness: Unaware found";
    }

    DPI_AWARENESS dpi_awareness =
        GetAwarenessFromDpiAwarenessContext(awareness_context);

    switch (dpi_awareness) {
      // Scale the window to the system DPI
      case DPI_AWARENESS_SYSTEM_AWARE:
        if (UINT uDpi = GetDpiForSystem()) {
          VLOG(2) << "DPI: Use Dpi in System Awareness";
          return uDpi;
        }
        break;

      // Scale the window to the monitor DPI
      case DPI_AWARENESS_PER_MONITOR_AWARE:
        if (UINT uDpi = GetDpiForWindow(hWnd)) {
          VLOG(2) << "DPI: Use Dpi in Per Monitor Aware";
          return uDpi;
        }
        break;
      case DPI_AWARENESS_UNAWARE:
        VLOG(2) << "DPI: Use Dpi in Unware";
        return USER_DEFAULT_SCREEN_DPI;
        break;
      case DPI_AWARENESS_INVALID:
        VLOG(2) << "DPI: Dpi in Invalid";
        break;
      default:
        VLOG(2) << "DPI: Dpi in Unknown";
        break;
    }
  }

  VLOG(2) << "DpiAwarenessContext is not found, falling back...";
  UINT xdpi, ydpi;
  HMONITOR hMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);
  if (hMonitor &&
      SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi))) {
    VLOG(2) << "DPI: Use Dpi in Monitor";
    return static_cast<unsigned int>(ydpi);
  }

  VLOG(2) << "DpiAwarenessMonitor is not found, falling back...";

  HDC hDC = ::GetDC(hWnd);
  ydpi = GetDeviceCaps(hDC, LOGPIXELSY);
  ::ReleaseDC(nullptr, hDC);

  return ydpi;
}

bool Utils::EnableNonClientDpiScaling(HWND hWnd) {
  return ::EnableNonClientDpiScaling(hWnd) == TRUE;
}

bool Utils::SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) {
  return ::SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, dpi) == TRUE;
}

bool Utils::GetUserDefaultLocaleName(std::wstring* localeName) {
  wchar_t localeNameBuf[LOCALE_NAME_MAX_LENGTH];
  // Returns the size of the buffer containing the locale name, including the terminating null character, if successful.
  int localNameLen = ::GetUserDefaultLocaleName(localeNameBuf, sizeof(localeNameBuf)/sizeof(localeNameBuf[0]));
  if (localNameLen == 0 || localNameLen == 1) {
    localeName->clear();
    return false;
  }
  *localeName = std::wstring(localeNameBuf, localNameLen - 1);
  return true;
}

static int add_to_auto_start(const wchar_t* appname_w, const std::wstring& cmdline) {
  HKEY hkey;

  if (!OpenWritableKey(&hkey)) {
    return -1;
  }
  ScopedHKEY hk(hkey);

  // For string-based types, such as REG_SZ, the string must be null-terminated.
  // If the data is of type REG_SZ, REG_EXPAND_SZ, or REG_MULTI_SZ,
  // cbData must include the size of the terminating null character or characters.
  DWORD n = sizeof(wchar_t) * (cmdline.size() + 1);
  LSTATUS result = ::RegSetValueExW(hkey      /* HKEY */,
                                    appname_w /* lpValueName */,
                                    0         /* Reserved */,
                                    REG_SZ    /* dwType */,
                                    reinterpret_cast<const BYTE*>(cmdline.c_str()) /* lpData */,
                                    n         /* cbData */);

  if (result != ERROR_SUCCESS) {
    return -1;
  }

  VLOG(1) << "[autostart] written autostart entry: " << SysWideToUTF8(cmdline);

  return 0;
}

static int delete_from_auto_start(const wchar_t* appname) {
  HKEY hkey;

  if (!OpenWritableKey(&hkey)) {
    return -1;
  }
  ScopedHKEY hk(hkey);

  LSTATUS result = ::RegDeleteValueW(hkey /* HKEY */, appname /* lpValueName */);

  if (result != ERROR_SUCCESS) {
    return -1;
  }

  VLOG(1) << "[autostart] removed autostart entry";

  return 0;
}

static std::wstring GetAutoStartCmdline() {
  std::wostringstream ss;
  std::wstring cmdline;

  /* turn on auto start  */
  if (!GetExecutablePathW(&cmdline)) {
    LOG(FATAL) << "GetExecutablePathW failed";
  }

  ss << L"\"" << cmdline << L"\" --background";
  return ss.str();
}

static int get_yass_auto_start() {
  HKEY hkey;

  if (!OpenReadableKey(&hkey)) {
    return -1;
  }
  ScopedHKEY hk(hkey);

  std::wstring output;
  const wchar_t* valueName = DEFAULT_AUTOSTART_NAME;
  DWORD BufferSize;
  DWORD type;

  // If lpData is nullptr, and lpcbData is non-nullptr, the function returns
  // ERROR_SUCCESS and stores the size of the data, in bytes, in the variable
  // pointed to by lpcbData. This enables an application to determine the best
  // way to allocate a buffer for the value's data.
  if (::RegQueryValueExW(hkey /* HKEY */, valueName /* lpValueName */,
                         nullptr /* lpReserved */, &type /* lpType */,
                         nullptr /* lpData */,
                         &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    /* yass applet auto start no set  */
    VLOG(1) << "[autostart] no auto start entry set";
    return -1;
  }

  if (type != REG_SZ || BufferSize > kRegReadMaximumSize ||
      BufferSize % sizeof(wchar_t) != 0) {
    VLOG(1) << "[autostart] mistyped auto start entry set";
    return -1;
  }

  output.resize(BufferSize / sizeof(wchar_t) + 2);
  if (::RegQueryValueExW(hkey /* HKEY */,
                         valueName /* lpValueName */,
                         nullptr /* lpReserved */,
                         &type /* lpType */,
                         reinterpret_cast<BYTE*>(output.data()) /* lpData */,
                         &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
    VLOG(1) << "[autostart] failed to fetch auto start entry";
    return -1;
  }
  // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type,
  // the string may not have been stored with the proper terminating null characters.
  // Therefore, even if the function returns ERROR_SUCCESS,
  // the application should ensure that the string is properly terminated before using it
  output.reserve(BufferSize / sizeof(wchar_t));
  // removing trailing space
  while (!output.empty() && output.back() == L'\0') {
    output.resize(output.size()-1);
  }

  VLOG(2) << "[autostart] previous autostart entry: " << SysWideToUTF8(output);

  if (GetAutoStartCmdline() != output) {
    return -1;
  }

  VLOG(1) << "[autostart] previous autostart entry matches current one";

  return 0;
}

static int set_yass_auto_start(bool on) {
  int result = 0;
  if (on) {
    /* turn on auto start  */
    result = add_to_auto_start(DEFAULT_AUTOSTART_NAME, GetAutoStartCmdline());
  } else {
    /* turn off auto start */
    result = delete_from_auto_start(DEFAULT_AUTOSTART_NAME);
  }
  return result;
}

bool Utils::GetAutoStart() {
  (void)EnsureKernel32Loaded();
  return get_yass_auto_start() == 0;
}

void Utils::EnableAutoStart(bool on) {
  set_yass_auto_start(on);
}

bool Utils::GetSystemProxy() {
  bool enabled;
  std::string server_addr, bypass_addr;
  if (!QuerySystemProxy(&enabled, &server_addr, &bypass_addr)) {
      return false;
  }
  VLOG(2) << "[system proxy] previous enabled: " << std::boolalpha << enabled
    << " server addr: " << server_addr << " bypass addr: " << bypass_addr;
  return enabled && server_addr == GetLocalAddr();
}

std::string Utils::GetLocalAddr() {
  std::ostringstream ss;
  auto local_host = absl::GetFlag(FLAGS_local_host);
  auto local_port = absl::GetFlag(FLAGS_local_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(local_host.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address && addr.is_v6()) {
    ss << "http://[" << local_host << "]:" << local_port;
  } else {
    ss << "http://" << local_host << ":" << local_port;
  }
  return ss.str();
}

bool Utils::SetSystemProxy(bool on) {
  bool enabled;
  std::string server_addr, bypass_addr = "<local>";
  ::QuerySystemProxy(&enabled, &server_addr, &bypass_addr);
  if (on) {
    server_addr = GetLocalAddr();
  }
  return ::SetSystemProxy(on, server_addr, bypass_addr);
}

std::wstring LoadStringStdW(HINSTANCE hInstance, UINT uID) {
  // The buffer to receive a read-only pointer to the string resource itself (if
  // cchBufferMax is zero).
  void* ptr;
  int len = ::LoadStringW(hInstance, uID, reinterpret_cast<wchar_t*>(&ptr), 0);
  // The string resource is not guaranteed to be null-terminated in the
  // module's resource table,
  std::wstring str(len, L'\0');
  // The number of characters copied into the buffer (if cchBufferMax is non-zero),
  // not including the terminating null character.
  ::LoadStringW(hInstance, uID, str.data(), len + 1);
  return str;
}

bool QuerySystemProxy(bool *enabled,
                      std::string *server_addr,
                      std::string *bypass_addr) {
  std::vector<INTERNET_PER_CONN_OPTIONW> options;
  options.resize(3);
  options[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;
  options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
  options[1].Value.pszValue = nullptr;
  options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
  options[2].Value.pszValue = nullptr;

  INTERNET_PER_CONN_OPTION_LISTW option_list;
  option_list.dwSize = sizeof(option_list);
  option_list.pszConnection = nullptr;
  option_list.dwOptionCount = options.size();
  option_list.dwOptionError = 0;
  option_list.pOptions = &options[0];
  DWORD option_list_size = sizeof(option_list);

  if (!::InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                              &option_list, &option_list_size)) {
    option_list_size = sizeof(option_list);
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    if (!::InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                                &option_list, &option_list_size)) {
      PLOG(WARNING)<< "Failed to query system proxy";
      return false;
    }
  }
  *enabled = !!(options[0].Value.dwValue & PROXY_TYPE_PROXY);
  if (options[1].Value.pszValue) {
    auto temp = options[1].Value.pszValue;
    *server_addr = temp ? SysWideToUTF8(temp) : std::string();
    if (temp) {
      GlobalFree(temp);
    }
  }
  if (options[2].Value.pszValue) {
    auto temp = options[2].Value.pszValue;
    *bypass_addr = temp ? SysWideToUTF8(temp) : std::string();
    if (temp) {
      GlobalFree(temp);
    }
  }
  return true;
}

bool SetSystemProxy(bool enable,
                    const std::string &server_addr,
                    const std::string &bypass_addr) {
  std::vector<std::wstring> conn_names;
  if (!::GetAllRasConnection(&conn_names)) {
    return false;
  }
  // Insert an empty RAS connection, happens in wine environment.
  conn_names.insert(conn_names.begin(), 1, std::wstring());
  bool ret = true;
  for (const auto& conn_name : conn_names) {
    ret &= ::SetSystemProxy(enable, server_addr, bypass_addr, conn_name);
  }
  return ret;
}

bool SetSystemProxy(bool enable,
                    const std::string &server_addr,
                    const std::string &bypass_addr,
                    const std::wstring &wconn_name) {
  std::wstring wserver_addr = SysUTF8ToWide(server_addr);
  std::wstring wbypass_addr = SysUTF8ToWide(bypass_addr);

  std::vector<INTERNET_PER_CONN_OPTIONW> options;
  options.resize(enable ? 3 : 1);
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  if (enable) {
    options[0].Value.dwValue = PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT;
    options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    options[1].Value.pszValue = const_cast<wchar_t*>(wserver_addr.c_str());
    options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    options[2].Value.pszValue = const_cast<wchar_t*>(wbypass_addr.c_str());
  } else {
    options[0].Value.dwValue = PROXY_TYPE_DIRECT;
  }

  INTERNET_PER_CONN_OPTION_LISTW option_list;
  option_list.dwSize = sizeof(option_list);
  option_list.pszConnection = wconn_name.empty() ? nullptr : const_cast<wchar_t*>(wconn_name.c_str());
  option_list.dwOptionCount = options.size();
  option_list.dwOptionError = 0;
  option_list.pOptions = &options[0];

  std::string conn_name = SysWideToUTF8(wconn_name);
  if (conn_name.empty()) {
    conn_name = "(empty)";
  }
  if (!::InternetSetOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                            &option_list, sizeof(option_list))) {
    PLOG(WARNING)<< "Failed to set system proxy"
      << " in connection \"" << conn_name << "\".";
    return false;
  }
  if (!::InternetSetOptionW(nullptr, INTERNET_OPTION_PROXY_SETTINGS_CHANGED,
                            nullptr, 0)) {
    PLOG(WARNING)<< "Failed to refresh system proxy"
      << " in connection \"" << conn_name << "\".";
    return false;
  }
  if (!::InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH,
                            nullptr, 0)) {
    PLOG(WARNING)<< "Failed to reload via system proxy"
      << " in connection \"" << conn_name << "\".";
    return false;
  }
  if (enable) {
    LOG(INFO) << "Set system proxy to " << server_addr
      << " by pass " << bypass_addr
      << " in connection \"" << conn_name << "\".";
  } else {
    LOG(INFO) << "Set system proxy disabled"
      << " in connection \"" << conn_name << "\".";
  }
  return true;
}

bool GetAllRasConnection(std::vector<std::wstring> *result) {
  DWORD dwCb = 0;
  DWORD dwRet = ERROR_SUCCESS;
  DWORD dwEntries = 0;
  LPRASENTRYNAMEW lpRasEntryName = nullptr;
  constexpr int kStaticRasEntryNumber = 30;

  RASENTRYNAMEW rasEntryNames[kStaticRasEntryNumber] = {};
  dwCb = sizeof(RASENTRYNAMEW) * kStaticRasEntryNumber;
  dwEntries = kStaticRasEntryNumber;
  lpRasEntryName = &rasEntryNames[0];

  for (DWORD i = 0; i < dwEntries; ++i) {
    lpRasEntryName[i].dwSize = sizeof(RASENTRYNAMEW);
  }

  // Call RasEnumEntries with lpRasEntryName = NULL. dwCb is returned with the required buffer size and
  // a return code of ERROR_BUFFER_TOO_SMALL
  dwRet = RasEnumEntriesW(nullptr, nullptr, lpRasEntryName, &dwCb, &dwEntries);
  result->clear();

  if (dwRet == ERROR_BUFFER_TOO_SMALL) {
    if (dwCb != dwEntries * sizeof(RASENTRYNAMEW)) {
      LOG(WARNING) << "RasEnumEntriesA: mismatched dwCb and dwEntries";
      return false;
    }
    // Allocate the memory needed for the array of RAS entry names.
    lpRasEntryName = reinterpret_cast<RASENTRYNAMEW*>(malloc(sizeof(RASENTRYNAMEW) * dwEntries));
    if (lpRasEntryName == nullptr){
      LOG(WARNING) << "RasEnumEntries: not enough memory";
      return false;
    }
    // The first RASENTRYNAME structure in the array must contain the structure size
    for (DWORD i = 0; i < dwEntries; ++i) {
      lpRasEntryName[i].dwSize = sizeof(RASENTRYNAMEW);
    }

    // Call RasEnumEntries to enumerate all RAS entry names
    dwRet = RasEnumEntriesW(nullptr, nullptr, lpRasEntryName, &dwCb, &dwEntries);

    // If successful, print the RAS entry names
    if (dwRet == ERROR_SUCCESS){
      for (DWORD i = 0; i < dwEntries; i++){
        result->emplace_back(lpRasEntryName[i].szEntryName);
      }
    } else {
      PLOG(WARNING) << "RasEnumEntries failed";
    }
    // Deallocate memory for the connection buffer
    free(lpRasEntryName);
    lpRasEntryName = nullptr;
  } else if (dwRet == ERROR_SUCCESS) {
    for (DWORD i = 0; i < dwEntries; i++){
      result->emplace_back(lpRasEntryName[i].szEntryName);
    }
  } else {
    PLOG(WARNING) << "RasEnumEntries failed";
  }
  if (dwRet == ERROR_SUCCESS && result->empty()) {
    LOG(INFO) << "RasEnumEntries: there were no RAS entry names found";
  }
  return dwRet == ERROR_SUCCESS;
}

#endif  // _WIN32
