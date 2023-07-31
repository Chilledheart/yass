// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#ifndef YASS_WIN32_UTILS
#define YASS_WIN32_UTILS
#include <cstdint>
#include <string>

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
  static bool GetSystemProxy();
  static bool SetSystemProxy(bool on);
  // http://127.0.0.1:1081
  static std::string GetLocalAddr();
};

std::wstring LoadStringStdW(HINSTANCE hInstance, UINT uID);

#define DEFAULT_AUTOSTART_NAME "YASS"

// server_addr and bypass_addr should be something like
// server_addr http://127.0.0.1:1081 bypass_addr <local>
// server_addr http=127.0.0.1:1081;https=127.0.0.1:1081;ftp=127.0.0.1:1081;socks=127.0.0.1:1081 bypass_addr <local>
bool QuerySystemProxy(bool *enabled,
                      std::string *server_addr,
                      std::string *bypass_addr);

bool SetSystemProxy(bool enable,
                    const std::string &server_addr,
                    const std::string &bypass_addr);

#endif  // YASS_WIN32_UTILS
