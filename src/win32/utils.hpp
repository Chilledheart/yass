// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_UTILS
#define YASS_WIN32_UTILS
#include <cstdint>
#include <string>

#include <windows.h>

class Utils {
 public:
  enum DpiAwarenessType {
    DpiAwarenessUnware,
    DpiAwarenessSystem,
    DpiAwarenessPerMonitor,
    DpiAwarenessPerMonitorV2,
  };
  static bool SetDpiAwareness(
      DpiAwarenessType awareness_type = DpiAwarenessPerMonitorV2);
  static bool SetMixedThreadDpiHostingBehavior();
  // Determine the DPI to use, according to the DPI awareness mode
  static unsigned int GetDpiForWindowOrSystem(HWND hWnd);
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
  static bool GetExecutablePath(std::wstring* executable_path);
};

inline std::wstring LoadStringStdW(HINSTANCE hInstance, UINT uID) {
  void* ptr;
  // The buffer to receive a read-only pointer to the string resource itself (if
  // cchBufferMax is zero).
  int len = ::LoadStringW(hInstance, uID, reinterpret_cast<wchar_t*>(&ptr), 0);
  // The string resource is not guaranteed to be null-terminated in the
  // module's resource table,
  std::wstring str(len + 1, L'\0');
  ::LoadStringW(hInstance, uID, const_cast<wchar_t*>(str.c_str()), str.size());
  return str;
}

#define DEFAULT_AUTOSTART_NAME "YASS"

#endif  // YASS_WIN32_UTILS
