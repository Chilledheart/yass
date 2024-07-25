// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "gtk/yass_window.hpp"

#include <sstream>

#include <absl/flags/flag.h>
#include <glib/gi18n.h>

#include "cli/cli_connection_stats.hpp"
#include "core/utils.hpp"
#include "feature.h"
#include "freedesktop/utils.hpp"
#include "gtk/option_dialog.hpp"
#include "gtk/utils.hpp"
#include "gtk/yass.hpp"
#include "gui_variant.h"
#include "version.h"

#ifdef HAVE_APP_INDICATOR
extern "C" int app_indicator_init();
extern "C" void app_indicator_uninit();
#include "third_party/libappindicator/app-indicator.h"
#endif

YASSWindow::YASSWindow() : impl_(GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL))) {
  gtk_window_set_title(GTK_WINDOW(impl_), YASS_APP_PRODUCT_NAME);
  gtk_window_set_position(GTK_WINDOW(impl_), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable(GTK_WINDOW(impl_), false);
  gtk_window_set_icon_name(GTK_WINDOW(impl_), "yass");

  auto show_callback = [](GtkWidget* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    gdk_window_set_functions(gtk_widget_get_window(GTK_WIDGET(window->impl_)),
                             static_cast<GdkWMFunction>(GDK_FUNC_MOVE | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE));
  };
  g_signal_connect(G_OBJECT(impl_), "show", G_CALLBACK(*show_callback), this);

  auto hide_callback = [](GtkWidget* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnClose();
  };
  g_signal_connect(G_OBJECT(impl_), "hide", G_CALLBACK(*hide_callback), this);

  // vbox, hbox
  GtkBox* vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  GtkGrid* grid = GTK_GRID(gtk_grid_new());
  gtk_grid_set_row_homogeneous(grid, true);

  // gtkmm's MenuBar/Menu/MenuItem is binded to model
  GtkWidget* menubar;
  GtkWidget* file_menu;
  GtkWidget* help_menu;
  GtkWidget* sep;
  GtkWidget* file_menu_item;
  GtkWidget* option_menu_item;
  GtkWidget* exit_menu_item;
  GtkWidget* help_menu_item;
  GtkWidget* about_menu_item;

  menubar = gtk_menu_bar_new();

  file_menu = gtk_menu_new();
  file_menu_item = gtk_menu_item_new_with_label(_("File"));
  option_menu_item = gtk_menu_item_new_with_label(_("Option..."));
  exit_menu_item = gtk_menu_item_new_with_label(_("Exit"));

  sep = gtk_separator_menu_item_new();

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), option_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), sep);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

  auto option_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnOption();
  };
  g_signal_connect(G_OBJECT(option_menu_item), "activate", G_CALLBACK(*option_callback), this);

  auto exit_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->close();
  };
  g_signal_connect(G_OBJECT(exit_menu_item), "activate", G_CALLBACK(*exit_callback), this);

  help_menu = gtk_menu_new();
  help_menu_item = gtk_menu_item_new_with_label(_("Help"));
  about_menu_item = gtk_menu_item_new_with_label(_("About..."));

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);

  auto about_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnAbout();
  };
  g_signal_connect(G_OBJECT(about_menu_item), "activate", G_CALLBACK(*about_callback), this);

  gtk_box_pack_start(vbox, menubar, FALSE, FALSE, 0);

  start_button_ = GTK_BUTTON(gtk_button_new());
  gtk_button_set_label(start_button_, _("Start"));
  gtk_widget_set_size_request(GTK_WIDGET(start_button_), 84, -1);

  stop_button_ = GTK_BUTTON(gtk_button_new());
  gtk_button_set_label(stop_button_, _("Stop"));
  gtk_widget_set_size_request(GTK_WIDGET(stop_button_), 84, -1);

  auto start_callback = [](GtkButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnStartButtonClicked();
  };

  g_signal_connect(G_OBJECT(start_button_), "clicked", G_CALLBACK(*start_callback), this);

  auto stop_callback = [](GtkButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnStopButtonClicked();
  };

  g_signal_connect(G_OBJECT(stop_button_), "clicked", G_CALLBACK(*stop_callback), this);

  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), false);

  gtk_grid_attach(grid, GTK_WIDGET(start_button_), 0, 2, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(stop_button_), 0, 7, 1, 1);

  auto server_host_label = gtk_label_new(_("Server Host"));
  auto server_sni_label = gtk_label_new(_("Server SNI"));
  auto server_port_label = gtk_label_new(_("Server Port"));
  auto username_label = gtk_label_new(_("Username"));
  auto password_label = gtk_label_new(_("Password"));
  auto method_label = gtk_label_new(_("Cipher/Method"));
  auto local_host_label = gtk_label_new(_("Local Host"));
  auto local_port_label = gtk_label_new(_("Local Port"));
  auto doh_url_label = gtk_label_new(_("DNS over HTTPS URL"));
  auto dot_host_label = gtk_label_new(_("DNS over TLS Host"));
  auto limit_rate_label = gtk_label_new(_("Limit Rate"));
  auto timeout_label = gtk_label_new(_("Timeout"));
  auto autostart_label = gtk_label_new(_("Auto Start"));
  auto systemproxy_label = gtk_label_new(_("System Proxy"));

  // see https://stackoverflow.com/questions/24994255/how-to-left-align-a-gtk-label-when-its-width-is-set-by-gtksizegroup
#if GTK_CHECK_VERSION(3, 16, 0)
  gtk_label_set_xalign(GTK_LABEL(server_host_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(server_sni_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(server_port_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(username_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(password_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(method_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(local_host_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(local_port_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(doh_url_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(dot_host_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(limit_rate_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(timeout_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(autostart_label), 0.0);
  gtk_label_set_xalign(GTK_LABEL(systemproxy_label), 0.0);
#else
  gtk_misc_set_alignment(GTK_MISC(server_host_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(server_host_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(server_sni_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(server_port_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(username_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(password_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(method_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(local_host_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(local_port_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(doh_url_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(dot_host_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(limit_rate_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(timeout_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(autostart_label), 0.0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(systemproxy_label), 0.0, 0.5);
#endif

  gtk_grid_attach(grid, GTK_WIDGET(server_host_label), 1, 0, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(server_sni_label), 1, 1, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(server_port_label), 1, 2, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(username_label), 1, 3, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(password_label), 1, 4, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(method_label), 1, 5, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(local_host_label), 1, 6, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(local_port_label), 1, 7, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(doh_url_label), 1, 8, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(dot_host_label), 1, 9, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(limit_rate_label), 1, 10, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(timeout_label), 1, 11, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(autostart_label), 1, 12, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(systemproxy_label), 1, 13, 1, 1);

  server_host_ = GTK_ENTRY(gtk_entry_new());
  server_sni_ = GTK_ENTRY(gtk_entry_new());
  server_port_ = GTK_ENTRY(gtk_entry_new());
  username_ = GTK_ENTRY(gtk_entry_new());
  password_ = GTK_ENTRY(gtk_entry_new());
  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };

  method_ = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());

  for (const char* method_name : method_names) {
    gtk_combo_box_text_append_text(method_, method_name);
  }
  local_host_ = GTK_ENTRY(gtk_entry_new());
  local_port_ = GTK_ENTRY(gtk_entry_new());
  doh_url_ = GTK_ENTRY(gtk_entry_new());
  dot_host_ = GTK_ENTRY(gtk_entry_new());
  limit_rate_ = GTK_ENTRY(gtk_entry_new());
  timeout_ = GTK_ENTRY(gtk_entry_new());

  autostart_ = GTK_CHECK_BUTTON(gtk_check_button_new());

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autostart_), Utils::GetAutoStart());

  auto checked_auto_start_callback = [](GtkToggleButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnAutoStartClicked();
  };

  g_signal_connect(G_OBJECT(autostart_), "toggled", G_CALLBACK(*checked_auto_start_callback), this);

  systemproxy_ = GTK_CHECK_BUTTON(gtk_check_button_new());

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(systemproxy_), Utils::GetSystemProxy());

  auto checked_system_proxy_callback = [](GtkToggleButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnSystemProxyClicked();
  };

  g_signal_connect(G_OBJECT(systemproxy_), "toggled", G_CALLBACK(*checked_system_proxy_callback), this);

  gtk_entry_set_visibility(password_, false);

  gtk_grid_attach(grid, GTK_WIDGET(server_host_), 2, 0, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(server_sni_), 2, 1, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(server_port_), 2, 2, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(username_), 2, 3, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(password_), 2, 4, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(method_), 2, 5, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(local_host_), 2, 6, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(local_port_), 2, 7, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(doh_url_), 2, 8, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(dot_host_), 2, 9, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(limit_rate_), 2, 10, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(timeout_), 2, 11, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(autostart_), 2, 12, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(systemproxy_), 2, 13, 1, 1);

  gtk_widget_set_margin_top(GTK_WIDGET(grid), 12);
  gtk_widget_set_margin_bottom(GTK_WIDGET(grid), 12);

#if GTK_CHECK_VERSION(3, 12, 0)
  gtk_widget_set_margin_start(GTK_WIDGET(grid), 12);
  gtk_widget_set_margin_end(GTK_WIDGET(grid), 12);
#else
  gtk_widget_set_margin_left(GTK_WIDGET(grid), 12);
  gtk_widget_set_margin_right(GTK_WIDGET(grid), 12);
#endif
  gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

  gtk_box_pack_start(vbox, GTK_WIDGET(grid), true, false, 0);

  status_bar_ = GTK_STATUSBAR(gtk_statusbar_new());
  gtk_statusbar_remove_all(status_bar_, 0);
  gtk_statusbar_push(status_bar_, 0, _("READY"));

  gtk_box_pack_start(vbox, GTK_WIDGET(status_bar_), true, false, 0);

  gtk_container_add(GTK_CONTAINER(impl_), GTK_WIDGET(vbox));

  LoadChanges();

  gtk_widget_show_all(GTK_WIDGET(impl_));

#ifdef HAVE_APP_INDICATOR
  if (app_indicator_init() == 0) {
    LOG(INFO) << "libappindicator3 initialized";
    CreateAppIndicator();
    return;
  } else {
    LOG(WARNING) << "libappindicator3 not initialized";
  }
#endif
  CreateStatusIcon();
}

YASSWindow::~YASSWindow() {
  if (tray_icon_) {
    g_object_unref(G_OBJECT(tray_icon_));
    tray_icon_ = nullptr;
  }
#ifdef HAVE_APP_INDICATOR
  if (tray_indicator_) {
    g_object_unref(G_OBJECT(tray_indicator_));
    tray_indicator_ = nullptr;
  }
  app_indicator_uninit();
#endif
}

void YASSWindow::CreateStatusIcon() {
  // set try icon file
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  tray_icon_ = gtk_status_icon_new_from_icon_name("yass");
  G_GNUC_END_IGNORE_DEPRECATIONS

  // set popup menu for tray icon
  GtkWidget* sep;
  GtkWidget* option_menu_item;
  GtkWidget* exit_menu_item;

  GtkWidget* tray_menu = gtk_menu_new();
  option_menu_item = gtk_menu_item_new_with_label(_("Option..."));
  exit_menu_item = gtk_menu_item_new_with_label(_("Exit"));

  sep = gtk_separator_menu_item_new();

  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), option_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), sep);
  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), exit_menu_item);

  auto option_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnOption();
  };
  g_signal_connect(G_OBJECT(option_menu_item), "activate", G_CALLBACK(*option_callback), this);

  auto exit_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->close();
  };
  g_signal_connect(G_OBJECT(exit_menu_item), "activate", G_CALLBACK(*exit_callback), this);

  gtk_widget_show_all(tray_menu);

  // set tooltip
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_status_icon_set_tooltip_text(tray_icon_, _("Show"));
  G_GNUC_END_IGNORE_DEPRECATIONS

  auto show_callback = [](GtkStatusIcon* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->show();
    window->present();
  };
  g_signal_connect(G_OBJECT(tray_icon_), "activate", G_CALLBACK(*show_callback), this);

  auto popup_callback = [](GtkStatusIcon* self, guint button, guint activate_time, gpointer popup_menu) {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_menu_popup(GTK_MENU(popup_menu), nullptr, nullptr, gtk_status_icon_position_menu, self, button, activate_time);
    G_GNUC_END_IGNORE_DEPRECATIONS
  };
  g_signal_connect(G_OBJECT(tray_icon_), "popup-menu", G_CALLBACK(*popup_callback), tray_menu);

  gtk_menu_attach_to_widget(GTK_MENU(tray_menu), GTK_WIDGET(impl_), nullptr);
}

#ifdef HAVE_APP_INDICATOR
void YASSWindow::CreateAppIndicator() {
  // set try icon file
  tray_indicator_ = G_OBJECT(app_indicator_new("it.gui.yass", "yass", APP_INDICATOR_CATEGORY_APPLICATION_STATUS));
  app_indicator_set_status(APP_INDICATOR(tray_indicator_), APP_INDICATOR_STATUS_ACTIVE);

  // set popup menu for tray icon
  GtkWidget* show_menu_item;
  GtkWidget* sep;
  GtkWidget* option_menu_item;
  GtkWidget* exit_menu_item;

  GtkWidget* tray_menu = gtk_menu_new();
  show_menu_item = gtk_menu_item_new_with_label(_("Show"));
  option_menu_item = gtk_menu_item_new_with_label(_("Option..."));
  exit_menu_item = gtk_menu_item_new_with_label(_("Exit"));

  sep = gtk_separator_menu_item_new();

  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), show_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), option_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), sep);
  gtk_menu_shell_append(GTK_MENU_SHELL(tray_menu), exit_menu_item);

  auto show_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->show();
    window->present();
  };
  g_signal_connect(G_OBJECT(show_menu_item), "activate", G_CALLBACK(*show_callback), this);

  auto option_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnOption();
  };
  g_signal_connect(G_OBJECT(option_menu_item), "activate", G_CALLBACK(*option_callback), this);

  auto exit_callback = [](GtkMenuItem* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->close();
  };
  g_signal_connect(G_OBJECT(exit_menu_item), "activate", G_CALLBACK(*exit_callback), this);

  gtk_widget_show_all(tray_menu);

  app_indicator_set_secondary_activate_target(APP_INDICATOR(tray_indicator_), GTK_WIDGET(show_menu_item));
  app_indicator_set_menu(APP_INDICATOR(tray_indicator_), GTK_MENU(tray_menu));
}
#endif  // HAVE_APP_INDICATOR

void YASSWindow::show() {
  gtk_widget_show(GTK_WIDGET(impl_));
}

void YASSWindow::present() {
  gtk_window_present(GTK_WINDOW(impl_));
}

void YASSWindow::close() {
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (tray_icon_) {
    gtk_status_icon_set_visible(tray_icon_, FALSE);
  }
  G_GNUC_END_IGNORE_DEPRECATIONS
#ifdef HAVE_APP_INDICATOR
  if (tray_indicator_) {
    app_indicator_set_status(APP_INDICATOR(tray_indicator_), APP_INDICATOR_STATUS_PASSIVE);
  }
#endif
  gtk_window_close(GTK_WINDOW(impl_));
}

void YASSWindow::OnStartButtonClicked() {
  gtk_widget_set_sensitive(GTK_WIDGET(start_button_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), false);

  gtk_widget_set_sensitive(GTK_WIDGET(server_host_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(server_sni_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(server_port_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(username_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(password_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(local_host_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(local_port_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(doh_url_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(dot_host_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(limit_rate_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(timeout_), false);

  mApp->OnStart();
}

void YASSWindow::OnStopButtonClicked() {
  gtk_widget_set_sensitive(GTK_WIDGET(start_button_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), false);

  mApp->OnStop();
}

void YASSWindow::OnAutoStartClicked() {
  Utils::EnableAutoStart(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autostart_)));
}

void YASSWindow::OnSystemProxyClicked() {
  Utils::SetSystemProxy(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(systemproxy_)));
}

std::string YASSWindow::GetServerHost() {
  return gtk_entry_get_text(server_host_);
}

std::string YASSWindow::GetServerSNI() {
  return gtk_entry_get_text(server_sni_);
}

std::string YASSWindow::GetServerPort() {
  return gtk_entry_get_text(server_port_);
}

std::string YASSWindow::GetUsername() {
  return gtk_entry_get_text(username_);
}

std::string YASSWindow::GetPassword() {
  return gtk_entry_get_text(password_);
}

std::string YASSWindow::GetMethod() {
  gchar* active_method = gtk_combo_box_text_get_active_text(method_);

  return make_unique_ptr_gfree(active_method).get();
}

std::string YASSWindow::GetLocalHost() {
  return gtk_entry_get_text(local_host_);
}

std::string YASSWindow::GetLocalPort() {
  return gtk_entry_get_text(local_port_);
}

std::string YASSWindow::GetDoHUrl() {
  return gtk_entry_get_text(doh_url_);
}

std::string YASSWindow::GetDoTHost() {
  return gtk_entry_get_text(dot_host_);
}

std::string YASSWindow::GetLimitRate() {
  return gtk_entry_get_text(limit_rate_);
}

std::string YASSWindow::GetTimeout() {
  return gtk_entry_get_text(timeout_);
}

std::string YASSWindow::GetStatusMessage() {
  if (mApp->GetState() != YASSApp::STARTED) {
    return mApp->GetStatus();
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = net::cli::total_rx_bytes;
    uint64_t tx_bytes = net::cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time * NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time * NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  ss << mApp->GetStatus();
  ss << _(" tx rate: ");
  HumanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << _(" rx rate: ");
  HumanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return ss.str();
}

void YASSWindow::Started() {
  UpdateStatusBar();
  gtk_widget_set_sensitive(GTK_WIDGET(start_button_), false);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();

  gtk_widget_set_sensitive(GTK_WIDGET(start_button_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), false);

  gtk_widget_set_sensitive(GTK_WIDGET(server_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(server_sni_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(server_port_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(username_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(password_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(local_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(local_port_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(doh_url_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(dot_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(limit_rate_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(timeout_), true);

  GtkDialog* alert_dialog = GTK_DIALOG(gtk_message_dialog_new(impl_, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                              GTK_BUTTONS_OK, "%s", mApp->GetStatus().c_str()));

  gtk_dialog_run(GTK_DIALOG(alert_dialog));
  gtk_widget_destroy(GTK_WIDGET(alert_dialog));
}

void YASSWindow::Stopped() {
  UpdateStatusBar();

  gtk_widget_set_sensitive(GTK_WIDGET(start_button_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), false);

  gtk_widget_set_sensitive(GTK_WIDGET(server_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(server_sni_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(server_port_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(username_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(password_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(local_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(local_port_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(doh_url_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(dot_host_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(limit_rate_), true);
  gtk_widget_set_sensitive(GTK_WIDGET(timeout_), true);
}

void YASSWindow::LoadChanges() {
  auto server_host_str = absl::GetFlag(FLAGS_server_host);
  auto server_sni_str = absl::GetFlag(FLAGS_server_sni);
  auto server_port_str = std::to_string(absl::GetFlag(FLAGS_server_port));
  auto username_str = absl::GetFlag(FLAGS_username);
  auto password_str = absl::GetFlag(FLAGS_password);
  uint32_t cipher_method = absl::GetFlag(FLAGS_method).method;
  auto local_host_str = absl::GetFlag(FLAGS_local_host);
  auto local_port_str = std::to_string(absl::GetFlag(FLAGS_local_port));
  auto doh_url_str = absl::GetFlag(FLAGS_doh_url);
  auto dot_host_str = absl::GetFlag(FLAGS_dot_host);
  std::string limit_rate_str = absl::GetFlag(FLAGS_limit_rate);
  auto timeout_str = std::to_string(absl::GetFlag(FLAGS_connect_timeout));

  gtk_entry_set_text(server_host_, server_host_str.c_str());
  gtk_entry_set_text(server_sni_, server_sni_str.c_str());
  gtk_entry_set_text(server_port_, server_port_str.c_str());
  gtk_entry_set_text(username_, username_str.c_str());
  gtk_entry_set_text(password_, password_str.c_str());

  static const uint32_t method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  unsigned int i;
  for (i = 0; i < std::size(method_ids); ++i) {
    if (cipher_method == method_ids[i])
      break;
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(method_), i);

  gtk_entry_set_text(local_host_, local_host_str.c_str());
  gtk_entry_set_text(local_port_, local_port_str.c_str());
  gtk_entry_set_text(doh_url_, doh_url_str.c_str());
  gtk_entry_set_text(dot_host_, dot_host_str.c_str());
  gtk_entry_set_text(limit_rate_, limit_rate_str.c_str());
  gtk_entry_set_text(timeout_, timeout_str.c_str());
}

void YASSWindow::UpdateStatusBar() {
  std::string status_msg = GetStatusMessage();
  if (last_status_msg_ == status_msg) {
    return;
  }
  last_status_msg_ = status_msg;
  gtk_statusbar_remove_all(status_bar_, 0);
  gtk_statusbar_push(status_bar_, 0, last_status_msg_.c_str());
}

void YASSWindow::OnOption() {
  OptionDialog option_dialog(_("YASS Option"), nullptr, true);

  option_dialog.run();
}

void YASSWindow::OnAbout() {
  GtkAboutDialog* about_dialog = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
  const char* artists[] = {"macosicons.com", nullptr};
  gtk_about_dialog_set_artists(about_dialog, artists);
  const char* authors[] = {YASS_APP_COMPANY_NAME, nullptr};
  gtk_about_dialog_set_authors(about_dialog, authors);
  std::string comments = _("Last Change: ");
  comments += YASS_APP_LAST_CHANGE;
  comments += "\n";
  comments += _("Enabled Feature: ");
  comments += YASS_APP_FEATURES;
  comments += "\n";
  comments += _("GUI Variant: ");
  comments += YASS_GUI_FLAVOUR;
  gtk_about_dialog_set_comments(about_dialog, comments.c_str());
  gtk_about_dialog_set_copyright(about_dialog, YASS_APP_COPYRIGHT);
  gtk_about_dialog_set_license_type(about_dialog, GTK_LICENSE_GPL_2_0_ONLY);
  gtk_about_dialog_set_logo_icon_name(about_dialog, "yass");
  gtk_about_dialog_set_program_name(about_dialog, YASS_APP_PRODUCT_NAME);
  gtk_about_dialog_set_version(about_dialog, YASS_APP_PRODUCT_VERSION);
  gtk_about_dialog_set_website(about_dialog, YASS_APP_WEBSITE);
  gtk_about_dialog_set_website_label(about_dialog, _("official-site"));
  gtk_window_set_position(GTK_WINDOW(about_dialog), GTK_WIN_POS_CENTER);
  gtk_dialog_run(GTK_DIALOG(about_dialog));
  gtk_widget_destroy(GTK_WIDGET(about_dialog));
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing ";
  mApp->Exit();
}
