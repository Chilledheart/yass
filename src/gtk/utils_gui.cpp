// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "gtk/utils.hpp"

#include "core/logging.hpp"

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "glibmm/fake_typeid.hpp"

#include <gtkmm/window.h>

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = ::getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

// returns true if the "file" exists and is a symbolic link
bool IsFile(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISREG(Stat.st_mode);
    }
  }
  return S_ISREG(Stat.st_mode);
}

// returns true if the "dir" exists and is a symbolic link
bool IsDirectory(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISDIR(Stat.st_mode);
    }
  }
  return S_ISDIR(Stat.st_mode);
}

bool CreatePrivateDirectory(const std::string& path) {
  return ::mkdir(path.c_str(), 0700) != 0;
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}

bool ReadFileToString(const std::string& path, std::string* context) {
  char buf[4096];
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = ::read(fd, buf, sizeof(buf) - 1);

  if (ret <= 0 || close(fd) < 0) {
    return false;
  }
  buf[ret] = '\0';
  *context = buf;
  return true;
}

bool WriteFileWithContent(const std::string& path, const std::string& context) {
  int fd = ::open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = ::write(fd, context.c_str(), context.size());

  if (ret != static_cast<long>(context.size()) || close(fd) < 0) {
    return false;
  }
  return true;
}

std::string GetAutostartDirectory() {
  const char* XdgConfigHome = getenv("XDG_CONFIG_HOME");
  if (!XdgConfigHome)
    XdgConfigHome = "~/.config";
  return ExpandUser(XdgConfigHome) + "/" + "autostart";
}
}  // namespace
bool Utils::GetAutoStart() {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  return IsFile(autostart_desktop);
}

void Utils::EnableAutoStart(bool on) {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  if (!on) {
    if (::unlink(autostart_desktop.c_str()) != 0) {
      LOG(WARNING) << "(Unexpected behavior): failed to unset autostart";
    }
  } else {
    // TODO: search XDG_DATA_DIRS instead of hard-coding which is like below
    // XDG_DATA_DIRS=/usr/share/gnome:/usr/local/share/:/usr/share/
    const char* origin_desktop =
        "/usr/share/applications/" DEFAULT_AUTOSTART_NAME ".desktop";
    std::string content;

    EnsureCreatedDirectory(GetAutostartDirectory());
    // force update, delete first
    if (IsFile(autostart_desktop)) {
      ::unlink(autostart_desktop.c_str());
    }

    // write to target
    if (!ReadFileToString(origin_desktop, &content) ||
        !WriteFileWithContent(autostart_desktop, content))  {
      LOG(WARNING) << "(Unexpected behavior): failed to set autostart";
    }
  }
}

void Utils::DisableGtkRTTI(Gtk::Window *window) {

  auto cobj = (Gtk::Container*)(window->gobj());

  GTK_CONTAINER_GET_CLASS(cobj)->add =
      [](GtkContainer* container, GtkWidget* widget) {
        const auto base = static_cast<GtkContainerClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(container))));
        if (base && base->add)
          base->add(container, widget);
      };
  GTK_CONTAINER_GET_CLASS(cobj)->remove =
      [](GtkContainer* container, GtkWidget* widget) {
        const auto base = static_cast<GtkContainerClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(container))));
        if (base && base->remove)
          base->remove(container, widget);
      };
  GTK_CONTAINER_GET_CLASS(cobj)->forall =
      [](GtkContainer* container, gboolean include_internals,
         GtkCallback callback, gpointer callback_data) {
        const auto base = static_cast<GtkContainerClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(container))));
        if (base && base->forall)
          base->forall(container, include_internals, callback, callback_data);
      };
  GTK_CONTAINER_GET_CLASS(cobj)->check_resize =
      [](GtkContainer* container) {
        const auto base = static_cast<GtkContainerClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(container))));
        if (base && base->check_resize)
          base->check_resize(container);
      };
  GTK_CONTAINER_GET_CLASS(cobj)->set_focus_child =
      [](GtkContainer* container, GtkWidget *p0) {
        const auto base = static_cast<GtkContainerClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(container))));
        if (base && base->set_focus_child)
          base->set_focus_child(container, p0);
      };

  auto wobj = (Gtk::Container*)(window->gobj());

  GTK_WIDGET_GET_CLASS(wobj)->show = [](GtkWidget* widget) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->show)
      base->show(widget);
  };
  GTK_WIDGET_GET_CLASS(wobj)->hide = [](GtkWidget* widget) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->hide)
      base->hide(widget);
  };
#ifdef GTKMM_ATKMM_ENABLED
  GTK_WIDGET_GET_CLASS(wobj)->get_accessible =
      [](GtkWidget* widget) -> AtkObject* {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->get_accessible)
      return base->get_accessible(widget);
    return nullptr;
  };
#endif
  GTK_WIDGET_GET_CLASS(wobj)->style_updated = [](GtkWidget* widget) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->style_updated)
      base->style_updated(widget);
  };
  GTK_WIDGET_GET_CLASS(wobj)->realize = [](GtkWidget* widget) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->realize)
      base->realize(widget);
  };
  GTK_WIDGET_GET_CLASS(wobj)->unrealize = [](GtkWidget* widget) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->unrealize)
      base->unrealize(widget);
  };
  GTK_WIDGET_GET_CLASS(wobj)->get_request_mode =
      [](GtkWidget* widget) -> GtkSizeRequestMode {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->get_request_mode)
      return base->get_request_mode(widget);
    return GtkSizeRequestMode();
  };
  GTK_WIDGET_GET_CLASS(wobj)->get_preferred_width =
      [](GtkWidget* widget, int* minimum_width, int* natural_width) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->get_preferred_width)
          base->get_preferred_width(widget, minimum_width, natural_width);
      };
  GTK_WIDGET_GET_CLASS(wobj)->get_preferred_height =
      [](GtkWidget* widget, int* minimum_height, int* natural_height) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->get_preferred_height)
          base->get_preferred_height(widget, minimum_height, natural_height);
      };
  GTK_WIDGET_GET_CLASS(wobj)->get_preferred_height_for_width =
      [](GtkWidget* widget, int width, int* minimum_height,
         int* natural_height) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->get_preferred_height_for_width)
          base->get_preferred_height_for_width(widget, width, minimum_height,
                                               natural_height);
      };
  GTK_WIDGET_GET_CLASS(wobj)->get_preferred_width_for_height =
      [](GtkWidget* widget, int height, int* minimum_width,
         int* natural_width) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->get_preferred_width_for_height)
          base->get_preferred_width_for_height(widget, height, minimum_width,
                                               natural_width);
      };
  GTK_WIDGET_GET_CLASS(wobj)->size_allocate =
      [](GtkWidget* widget, GdkRectangle* rect) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->size_allocate)
          base->size_allocate(widget, rect);
      };
  GTK_WIDGET_GET_CLASS(wobj)->map =
      [](GtkWidget* widget) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->map)
          base->map(widget);
      };
  GTK_WIDGET_GET_CLASS(wobj)->unmap =
      [](GtkWidget* widget) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->unmap)
          base->unmap(widget);
      };
  GTK_WIDGET_GET_CLASS(wobj)->map_event =
      [](GtkWidget* widget, GdkEventAny *p0) -> int {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->map_event)
          return base->map_event(widget, p0);
        return int();
      };
  GTK_WIDGET_GET_CLASS(wobj)->focus =
      [](GtkWidget* widget, GtkDirectionType p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->focus)
      return base->focus(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->event =
      [](GtkWidget* widget, GdkEvent* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->event)
      return base->event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->delete_event =
      [](GtkWidget* widget, GdkEventAny* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->delete_event)
      return base->delete_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->destroy_event =
      [](GtkWidget* widget, GdkEventAny* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->destroy_event)
      return base->destroy_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->window_state_event =
      [](GtkWidget* widget, GdkEventWindowState* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->window_state_event)
      return base->window_state_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->configure_event =
      [](GtkWidget* widget, GdkEventConfigure* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->configure_event)
      return base->configure_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->state_changed =
      [](GtkWidget* widget, GtkStateType p0) {
        const auto base = static_cast<GtkWidgetClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
        if (base && base->state_changed)
          base->state_changed(widget, p0);
      };
  GTK_WIDGET_GET_CLASS(wobj)->visibility_notify_event =
      [](GtkWidget* widget, GdkEventVisibility* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->visibility_notify_event)
      return base->visibility_notify_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->enter_notify_event =
      [](GtkWidget* widget, GdkEventCrossing* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->enter_notify_event)
      return base->enter_notify_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->leave_notify_event =
      [](GtkWidget* widget, GdkEventCrossing* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->leave_notify_event)
      return base->leave_notify_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->focus_in_event =
      [](GtkWidget* widget, GdkEventFocus* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->focus_in_event)
      return base->focus_in_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->draw =
      [](GtkWidget* widget, cairo_t* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->draw)
      return base->draw(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->button_press_event =
      [](GtkWidget* widget, GdkEventButton* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->button_press_event)
      return base->button_press_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->button_release_event =
      [](GtkWidget* widget, GdkEventButton* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->button_release_event)
      return base->button_release_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->motion_notify_event =
      [](GtkWidget* widget, GdkEventMotion* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->motion_notify_event)
      return base->motion_notify_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->focus_in_event =
      [](GtkWidget* widget, GdkEventFocus* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->focus_in_event)
      return base->focus_in_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->focus_out_event =
      [](GtkWidget* widget, GdkEventFocus* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->focus_out_event)
      return base->focus_out_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->key_press_event =
      [](GtkWidget* widget, GdkEventKey* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->key_press_event)
      return base->key_press_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->key_release_event =
      [](GtkWidget* widget, GdkEventKey* p0) -> gboolean {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->key_release_event)
      return base->key_release_event(widget, p0);
    return gboolean();
  };
  GTK_WIDGET_GET_CLASS(wobj)->grab_notify =
      [](GtkWidget* widget, gboolean p0) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->grab_notify)
      base->grab_notify(widget, p0);
  };
  GTK_WIDGET_GET_CLASS(wobj)->child_notify =
      [](GtkWidget* widget, GParamSpec *p0) {
    const auto base = static_cast<GtkWidgetClass*>(
        g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(widget))));
    if (base && base->child_notify)
      base->child_notify(widget, p0);
  };

  G_OBJECT_GET_CLASS(window->gobj())->dispose =
      [](GObject *obj) {
        const auto base = static_cast<GObjectClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(obj))));
        if (base && base->dispose)
          base->dispose(obj);
      };
  GTK_WINDOW_GET_CLASS(window->gobj())->set_focus =
      [](GtkWindow* window, GtkWidget* p0) {
        const auto base = static_cast<GtkWindowClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(window))));
        if (base && base->set_focus)
          base->set_focus(window, p0);
      };
  GTK_WINDOW_GET_CLASS(window->gobj())->activate_focus =
      [](GtkWindow* window) {
        const auto base = static_cast<GtkWindowClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(window))));
        if (base && base->activate_focus)
          base->activate_focus(window);
      };
  GTK_WINDOW_GET_CLASS(window->gobj())->activate_default =
      [](GtkWindow* window) {
        const auto base = static_cast<GtkWindowClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(window))));
        if (base && base->activate_default)
          base->activate_default(window);
      };
  GTK_WINDOW_GET_CLASS(window->gobj())->keys_changed =
      [](GtkWindow* window) {
        const auto base = static_cast<GtkWindowClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(window))));
        if (base && base->keys_changed)
          base->keys_changed(window);
      };
  GTK_WINDOW_GET_CLASS(window->gobj())->enable_debugging =
      [](GtkWindow* window, gboolean p0) -> gboolean {
        const auto base = static_cast<GtkWindowClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(window))));
        if (base && base->enable_debugging)
          return base->enable_debugging(window, p0);
        return gboolean();
      };
}
