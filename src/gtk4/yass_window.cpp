// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "gtk4/yass_window.hpp"

#include <sstream>

#include <absl/flags/flag.h>
#include <glib/gi18n.h>

#include "cli/cli_connection_stats.hpp"
#include "core/utils.hpp"
#include "freedesktop/utils.hpp"
#include "gtk/utils.hpp"
#include "gtk4/yass.hpp"
#include "version.h"

#include <gtk/gtkwindow.h>

extern "C" {

struct _YASSGtkWindow {
  GtkApplicationWindow parent;

  GtkWidget* gears;

  GtkWidget* status_bar;

  // Left Panel
  GtkWidget* start_button;
  GtkWidget* stop_button;

  // Right Panel
  GtkWidget* server_host;
  GtkWidget* server_sni;
  GtkWidget* server_port;
  GtkWidget* username;
  GtkWidget* password;
  GtkWidget* method;
  GtkWidget* local_host;
  GtkWidget* local_port;
  GtkWidget* doh_url;
  GtkWidget* dot_host;
  GtkWidget* timeout;
  GtkWidget* autostart;
  GtkWidget* systemproxy;
};

G_DEFINE_TYPE(YASSGtkWindow, yass_window, GTK_TYPE_APPLICATION_WINDOW)

static void yass_window_init(YASSGtkWindow* win) {
  GtkBuilder* builder;
  GMenuModel* menu;

  gtk_widget_init_template(GTK_WIDGET(win));

  builder = gtk_builder_new_from_resource("/it/gui/yass/menu.ui");
  menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(win->gears), menu);
  g_object_unref(builder);
}

static void yass_window_dispose(GObject* object) {
  YASSGtkWindow* window = YASS_WINDOW(object);
#if GTK_CHECK_VERSION(4, 8, 0)
  gtk_widget_dispose_template(GTK_WIDGET(window), yass_window_get_type());
#else
  gtk_widget_unparent(window->gears);

  gtk_widget_unparent(window->status_bar);

  gtk_widget_unparent(window->start_button);
  gtk_widget_unparent(window->stop_button);

  gtk_widget_unparent(window->server_host);
  gtk_widget_unparent(window->server_sni);
  gtk_widget_unparent(window->server_port);
  gtk_widget_unparent(window->username);
  gtk_widget_unparent(window->password);
  gtk_widget_unparent(window->method);
  gtk_widget_unparent(window->local_host);
  gtk_widget_unparent(window->local_port);
  gtk_widget_unparent(window->doh_url);
  gtk_widget_unparent(window->dot_host);
  gtk_widget_unparent(window->timeout);
  gtk_widget_unparent(window->autostart);
  gtk_widget_unparent(window->systemproxy);
#endif
  G_OBJECT_CLASS(yass_window_parent_class)->dispose(object);
}

static void yass_window_class_init(YASSGtkWindowClass* cls) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(cls), "/it/gui/yass/yass_window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, gears);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, status_bar);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, start_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, stop_button);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, server_host);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, server_sni);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, server_port);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, username);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, password);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, method);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, local_host);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, local_port);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, doh_url);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, dot_host);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, timeout);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, autostart);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, systemproxy);

  G_OBJECT_CLASS(cls)->dispose = yass_window_dispose;
}

YASSGtkWindow* yass_window_new(YASSGtkApp* app) {
  auto window = YASSGtk_WINDOW(g_object_new(yass_window_get_type(), "application", app, nullptr));
  gtk_window_set_resizable(GTK_WINDOW(window), false);
  gtk_window_set_icon_name(GTK_WINDOW(window), "yass");
  return window;
}

}  // extern "C"

YASSWindow::YASSWindow(GApplication* app) : app_(app), impl_(yass_window_new(YASSGtk_APP(app))) {
  static YASSWindow* window;
  window = this;

  // forward to hide event
  gtk_window_set_hide_on_close(GTK_WINDOW(impl_), TRUE);

  auto hide_callback = []() { window->OnClose(); };
  g_signal_connect(G_OBJECT(impl_), "hide", G_CALLBACK(hide_callback), this);

  auto start_callback = []() { window->OnStartButtonClicked(); };

  g_signal_connect(G_OBJECT(impl_->start_button), "clicked", G_CALLBACK(start_callback), nullptr);

  auto stop_callback = []() { window->OnStopButtonClicked(); };

  g_signal_connect(G_OBJECT(impl_->stop_button), "clicked", G_CALLBACK(stop_callback), nullptr);

  gtk_widget_set_sensitive(GTK_WIDGET(impl_->stop_button), false);

  auto autostart_callback = []() { window->OnAutoStartClicked(); };

  g_signal_connect(G_OBJECT(impl_->autostart), "toggled", G_CALLBACK(autostart_callback), nullptr);

  auto systemproxy_callback = []() { window->OnSystemProxyClicked(); };

  g_signal_connect(G_OBJECT(impl_->systemproxy), "toggled", G_CALLBACK(systemproxy_callback), nullptr);

  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };

  GtkComboBoxText* method = GTK_COMBO_BOX_TEXT(impl_->method);

  for (const char* method_name : method_names) {
    gtk_combo_box_text_append_text(method, method_name);
  }

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->autostart), Utils::GetAutoStart());

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->systemproxy), Utils::GetSystemProxy());

  gtk_entry_set_visibility(GTK_ENTRY(impl_->password), false);

  GtkStatusbar* status_bar = GTK_STATUSBAR(impl_->status_bar);
  gtk_statusbar_remove_all(status_bar, 0);
  gtk_statusbar_push(status_bar, 0, _("READY"));

  LoadChanges();
}

YASSWindow::~YASSWindow() {
  gtk_window_destroy(GTK_WINDOW(impl_));
}

void YASSWindow::show() {
  gtk_widget_set_visible(GTK_WIDGET(impl_), true);
}

void YASSWindow::present() {
  gtk_window_present(GTK_WINDOW(impl_));
}

void YASSWindow::close() {
  gtk_application_remove_window(GTK_APPLICATION(app_), GTK_WINDOW(impl_));
}

void YASSWindow::OnStartButtonClicked() {
  gtk_widget_set_sensitive(impl_->start_button, false);
  gtk_widget_set_sensitive(impl_->stop_button, false);

  gtk_widget_set_sensitive(impl_->server_host, false);
  gtk_widget_set_sensitive(impl_->server_sni, false);
  gtk_widget_set_sensitive(impl_->server_port, false);
  gtk_widget_set_sensitive(impl_->username, false);
  gtk_widget_set_sensitive(impl_->password, false);
  gtk_widget_set_sensitive(impl_->method, false);
  gtk_widget_set_sensitive(impl_->local_host, false);
  gtk_widget_set_sensitive(impl_->local_port, false);
  gtk_widget_set_sensitive(impl_->doh_url, false);
  gtk_widget_set_sensitive(impl_->dot_host, false);
  gtk_widget_set_sensitive(impl_->timeout, false);

  mApp->OnStart();
}

void YASSWindow::OnStopButtonClicked() {
  gtk_widget_set_sensitive(impl_->start_button, false);
  gtk_widget_set_sensitive(impl_->stop_button, false);

  mApp->OnStop();
}

void YASSWindow::OnAutoStartClicked() {
  Utils::EnableAutoStart(gtk_check_button_get_active(GTK_CHECK_BUTTON(impl_->autostart)));
}

void YASSWindow::OnSystemProxyClicked() {
  Utils::SetSystemProxy(gtk_check_button_get_active(GTK_CHECK_BUTTON(impl_->systemproxy)));
}

std::string YASSWindow::GetServerHost() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->server_host));
}

std::string YASSWindow::GetServerSNI() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->server_sni));
}

std::string YASSWindow::GetServerPort() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->server_port));
}

std::string YASSWindow::GetUsername() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->username));
}

std::string YASSWindow::GetPassword() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->password));
}

std::string YASSWindow::GetMethod() {
  gchar* active_method = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(impl_->method));

  return make_unique_ptr_gfree(active_method).get();
}

std::string YASSWindow::GetLocalHost() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->local_host));
}

std::string YASSWindow::GetLocalPort() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->local_port));
}

std::string YASSWindow::GetDoHUrl() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->doh_url));
}

std::string YASSWindow::GetDoTHost() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->dot_host));
}

std::string YASSWindow::GetTimeout() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->timeout));
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

  gtk_widget_set_sensitive(impl_->start_button, false);
  gtk_widget_set_sensitive(impl_->stop_button, true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();

  gtk_widget_set_sensitive(impl_->start_button, true);
  gtk_widget_set_sensitive(impl_->stop_button, false);

  gtk_widget_set_sensitive(impl_->server_host, true);
  gtk_widget_set_sensitive(impl_->server_sni, true);
  gtk_widget_set_sensitive(impl_->server_port, true);
  gtk_widget_set_sensitive(impl_->username, true);
  gtk_widget_set_sensitive(impl_->password, true);
  gtk_widget_set_sensitive(impl_->method, true);
  gtk_widget_set_sensitive(impl_->local_host, true);
  gtk_widget_set_sensitive(impl_->local_port, true);
  gtk_widget_set_sensitive(impl_->doh_url, true);
  gtk_widget_set_sensitive(impl_->dot_host, true);
  gtk_widget_set_sensitive(impl_->timeout, true);

  // Gtk4 Message Dialog is deprecated since 4.10
#if GTK_CHECK_VERSION(4, 10, 0)
  GtkAlertDialog* dialog = gtk_alert_dialog_new("%s", _("Start Failed"));
  const char* buttons[] = {_("OK"), nullptr};
  gtk_alert_dialog_set_detail(dialog, mApp->GetStatus().c_str());
  gtk_alert_dialog_set_buttons(dialog, buttons);
  gtk_window_present(GTK_WINDOW(impl_));
  gtk_alert_dialog_choose(dialog, GTK_WINDOW(impl_), nullptr, nullptr, nullptr);
#else
  GtkDialogFlags flags = (GtkDialogFlags)(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL);
  GtkDialog* dialog = GTK_DIALOG(gtk_message_dialog_new(GTK_WINDOW(impl_), flags, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                        "%s", mApp->GetStatus().c_str()));

  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), nullptr);
  gtk_widget_show(GTK_WIDGET(dialog));
#endif
}

void YASSWindow::Stopped() {
  UpdateStatusBar();

  gtk_widget_set_sensitive(impl_->start_button, true);
  gtk_widget_set_sensitive(impl_->stop_button, false);

  gtk_widget_set_sensitive(impl_->server_host, true);
  gtk_widget_set_sensitive(impl_->server_sni, true);
  gtk_widget_set_sensitive(impl_->server_port, true);
  gtk_widget_set_sensitive(impl_->username, true);
  gtk_widget_set_sensitive(impl_->password, true);
  gtk_widget_set_sensitive(impl_->method, true);
  gtk_widget_set_sensitive(impl_->local_host, true);
  gtk_widget_set_sensitive(impl_->local_port, true);
  gtk_widget_set_sensitive(impl_->doh_url, true);
  gtk_widget_set_sensitive(impl_->dot_host, true);
  gtk_widget_set_sensitive(impl_->timeout, true);
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
  auto timeout_str = std::to_string(absl::GetFlag(FLAGS_connect_timeout));

  gtk_editable_set_text(GTK_EDITABLE(impl_->server_host), server_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->server_sni), server_sni_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->server_port), server_port_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->username), username_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->password), password_str.c_str());

  static constexpr const uint32_t method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  unsigned int i;
  for (i = 0; i < std::size(method_ids); ++i) {
    if (cipher_method == method_ids[i])
      break;
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(impl_->method), i);

  gtk_editable_set_text(GTK_EDITABLE(impl_->local_host), local_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->local_port), local_port_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->doh_url), doh_url_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->dot_host), dot_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->timeout), timeout_str.c_str());
}

void YASSWindow::UpdateStatusBar() {
  std::string status_msg = GetStatusMessage();
  if (last_status_msg_ == status_msg) {
    return;
  }
  last_status_msg_ = status_msg;
  gtk_statusbar_remove_all(GTK_STATUSBAR(impl_->status_bar), 0);
  gtk_statusbar_push(GTK_STATUSBAR(impl_->status_bar), 0, last_status_msg_.c_str());
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing";
  mApp->Exit();
}
