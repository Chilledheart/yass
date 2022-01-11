// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
#ifndef YASS_GUI_UTILS
#define YASS_GUI_UTILS
#include <cstdint>
#include <string>

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
#ifdef _WIN32
  static bool SetProcessDpiAwareness();
#endif
};

#define DEFAULT_AUTOSTART_NAME "YASS"

#endif  // YASS_GUI_UTILS
