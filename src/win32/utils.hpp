// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
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
  static bool EnableNonClientDpiScaling(HWND hWnd);
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
};

std::wstring LoadStringStdW(HINSTANCE hInstance, UINT uID);

bool WriteCaBundleCrt(HINSTANCE hInstance, UINT uID);

#define DEFAULT_AUTOSTART_NAME "YASS"

#endif  // YASS_WIN32_UTILS
