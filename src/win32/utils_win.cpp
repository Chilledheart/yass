// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2023 Chilledheart  */

// We use dynamic loading for below functions
#define GetDeviceCaps GetDeviceCapsHidden
#define SetProcessDPIAware SetProcessDPIAwareHidden

#include "win32/utils.hpp"

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "config/config.hpp"

#ifdef _WIN32

#define DEFAULT_AUTOSTART_KEY \
  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"

#ifdef COMPILER_MSVC
#include <Tchar.h>
#else
#include <tchar.h>
#endif  // COMPILER_MSVC
#include <windows.h>
#include <wininet.h>

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

}  // namespace

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

namespace {
LONG get_win_run_key(HKEY* pKey) {
  const wchar_t* key_run = DEFAULT_AUTOSTART_KEY;
  LONG result =
      RegOpenKeyExW(HKEY_CURRENT_USER, key_run, 0L, KEY_WRITE | KEY_READ, pKey);

  return result;
}

int add_to_auto_start(const wchar_t* appname_w, const std::wstring& cmdline) {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  DWORD n = sizeof(wchar_t) * (cmdline.size() + 1);

  result = RegSetValueExW(hKey, appname_w, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(cmdline.c_str()), n);

  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  return 0;
}

int delete_from_auto_start(const wchar_t* appname) {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  result = RegDeleteValueW(hKey, appname);
  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  return 0;
}

int get_yass_auto_start() {
  HKEY hKey;
  LONG result = get_win_run_key(&hKey);
  if (result != ERROR_SUCCESS) {
    return -1;
  }

  char buf[MAX_PATH + 1] = {0};
  DWORD len = sizeof(buf);
  result = RegQueryValueExW(hKey,                          /* Key */
                            _T(DEFAULT_AUTOSTART_NAME),    /* value */
                            nullptr,                       /* reserved */
                            nullptr,                       /* output type */
                            reinterpret_cast<LPBYTE>(buf), /* output data */
                            &len);                         /* output length */

  RegCloseKey(hKey);
  if (result != ERROR_SUCCESS) {
    /* yass applet auto start no set  */
    return -1;
  }

  return 0;
}

int set_yass_auto_start(bool on) {
  int result = 0;
  if (on) {
    std::wstring cmdline;

    /* turn on auto start  */
    if (!GetExecutablePathW(&cmdline)) {
      return -1;
    }

    cmdline = L"\"" + cmdline + L"\" --background";

    result = add_to_auto_start(_T(DEFAULT_AUTOSTART_NAME), cmdline);

  } else {
    /* turn off auto start */
    result = delete_from_auto_start(_T(DEFAULT_AUTOSTART_NAME));
  }
  return result;
}

}  // namespace

bool Utils::GetAutoStart() {
  return get_yass_auto_start() == 0;
}

void Utils::EnableAutoStart(bool on) {
  set_yass_auto_start(on);
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
  std::vector<INTERNET_PER_CONN_OPTIONA> options;
  options.resize(3);
  options[0].dwOption = INTERNET_PER_CONN_FLAGS_UI;
  options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
  options[1].Value.pszValue = nullptr;
  options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
  options[2].Value.pszValue = nullptr;

  INTERNET_PER_CONN_OPTION_LISTA option_list;
  option_list.dwSize = sizeof(option_list);
  option_list.pszConnection = nullptr;
  option_list.dwOptionCount = options.size();
  option_list.dwOptionError = 0;
  option_list.pOptions = &options[0];
  DWORD option_list_size = sizeof(option_list);

  if (!::InternetQueryOptionA(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                              &option_list, &option_list_size)) {
    option_list_size = sizeof(option_list);
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    if (!::InternetQueryOptionA(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                                &option_list, &option_list_size)) {
      PLOG(WARNING)<< "Failed to query system proxy";
      return false;
    }
  }
  if (options[0].Value.dwValue & PROXY_TYPE_PROXY) {
    *enabled = true;
  }
  if (options[1].Value.pszValue) {
    *server_addr = options[1].Value.pszValue;
  }
  if (options[2].Value.pszValue) {
    *bypass_addr = options[2].Value.pszValue;
  }
  return true;
}

bool SetSystemProxy(bool enable,
                    const std::string &server_addr,
                    const std::string &bypass_addr) {
  std::vector<INTERNET_PER_CONN_OPTIONA> options;
  options.resize(3);
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  if (enable) {
    options[0].Value.dwValue = PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT;
  } else {
    options[0].Value.dwValue = PROXY_TYPE_DIRECT;
  }
  options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
  options[1].Value.pszValue = const_cast<char*>(server_addr.c_str());
  options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
  options[2].Value.pszValue = const_cast<char*>(bypass_addr.c_str());

  INTERNET_PER_CONN_OPTION_LISTA option_list;
  option_list.dwSize = sizeof(option_list);
  option_list.pszConnection = nullptr;
  option_list.dwOptionCount = options.size();
  option_list.dwOptionError = 0;
  option_list.pOptions = &options[0];

  if (!::InternetSetOptionA(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION,
                            &option_list, sizeof(option_list))) {
    PLOG(WARNING)<< "Failed to set system proxy";
    return false;
  }
  if (!::InternetSetOptionA(nullptr, INTERNET_OPTION_PROXY_SETTINGS_CHANGED,
                            nullptr, 0)) {
    PLOG(WARNING)<< "Failed to refresh system proxy";
    return false;
  }
  if (!::InternetSetOptionA(nullptr, INTERNET_OPTION_REFRESH,
                            nullptr, 0)) {
    PLOG(WARNING)<< "Failed to reload via system proxy";
    return false;
  }
  if (enable) {
    LOG(INFO) << "Set system proxy to " << server_addr
      << " by pass " << bypass_addr << ".";
  } else {
    LOG(INFO) << "Set system proxy disabled.";
  }
  return true;
}

#endif  // _WIN32
