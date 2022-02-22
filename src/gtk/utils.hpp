// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_GUI_UTILS
#define YASS_GUI_UTILS
#include <glib.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Gtk {
class Window;
} // namespace Gtk

class Utils {
 public:
  static bool GetAutoStart();
  static void EnableAutoStart(bool on);
  static void DisableGtkRTTI(Gtk::Window *window);
};

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
 GSource *source_ = nullptr;
 std::function<void()> callback_;
};

#define DEFAULT_AUTOSTART_NAME "yass"

#endif  // YASS_GUI_UTILS
