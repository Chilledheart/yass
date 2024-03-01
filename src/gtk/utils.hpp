// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#ifndef YASS_GUI_UTILS
#define YASS_GUI_UTILS
#include <glib.h>
#include <cstdint>
#include <functional>
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

template <typename T>
std::unique_ptr<T[], decltype(&g_free)> make_unique_ptr_gfree(T* p) {
  return std::unique_ptr<T[], decltype(&g_free)>(p, &g_free);
}

class Dispatcher {
 public:
  Dispatcher();
  ~Dispatcher();

  bool Init(std::function<void()> callback);
  bool Destroy();

  bool Emit();

 private:
  bool ReadCallback();

  int fds_[2] = {-1, -1};
  GSource* source_ = nullptr;
  std::function<void()> callback_;
};

void SetUpGLibLogHandler();

#endif  // YASS_GUI_UTILS
