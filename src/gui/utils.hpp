// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS
class Utils {
public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
};

#define DEFAULT_AUTOSTART_NAME "YASS"

#endif // YASS_UTILS
