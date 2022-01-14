// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_UTILS
#define YASS_WIN32_UTILS
#include <cstdint>
#include <string>

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
  static bool SetProcessDpiAwareness();
};

#define DEFAULT_AUTOSTART_NAME "YASS"

#endif  // YASS_WIN32_UTILS
