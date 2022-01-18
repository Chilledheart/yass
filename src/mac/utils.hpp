// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
#ifndef YASS_MAC_UTILS
#define YASS_MAC_UTILS
#include <cstdint>
#include <string>

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
};

#endif  // YASS_MAC_UTILS
