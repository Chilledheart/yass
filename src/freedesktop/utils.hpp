// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef YASS_FREEDESKTOP_UTILS
#define YASS_FREEDESKTOP_UTILS

#include <cstdint>
#include <memory>
#include <string>

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
  static bool GetSystemProxy();
  static bool SetSystemProxy(bool on);
  static std::string GetLocalAddr();
};

bool QuerySystemProxy(bool* enabled, std::string* server_host, std::string* server_port, std::string* bypass_addr);

bool SetSystemProxy(bool enable,
                    const std::string& server_host,
                    const std::string& server_port,
                    const std::string& bypass_addr);

bool QuerySystemProxy_KDE(bool* enabled, std::string* server_addr, std::string* bypass_addr);

bool SetSystemProxy_KDE(bool enable, const std::string& server_addr, const std::string& bypass_addr);

#endif  // YASS_FREEDESKTOP_UTILS
