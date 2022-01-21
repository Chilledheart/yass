// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_GUI_UTILS
#define YASS_GUI_UTILS
#include <cstdint>
#include <string>

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
};

#define DEFAULT_AUTOSTART_NAME "yass"

#endif  // YASS_GUI_UTILS
