// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
#ifndef YASS_UTILS
#define YASS_UTILS
#include <stdint.h>
#include <string>
#include <wx/string.h>
class Utils {
public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
  static bool SetProcessDpiAwareness();
  static uint64_t GetMonotonicTime();
  static int32_t Stoi(const std::string &value);
  static std::string ToString(const wxString &value);
};

#define DEFAULT_AUTOSTART_NAME "YASS"
#define NS_PER_SECOND (1000 * 1000 * 1000)

#endif // YASS_UTILS
