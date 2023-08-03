// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "gtk4/yass_window.hpp"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>

#include "cli/cli_connection_stats.hpp"
#include "core/utils.hpp"
#include "gtk4/utils.hpp"
#include "gtk4/yass.hpp"
#include "version.h"

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

extern "C" {

struct _YASSGtkWindow
{
  GtkApplicationWindow parent;

  GtkWidget *gears;

  GtkWidget* status_bar;

  // Left Panel
  GtkWidget* start_button;
  GtkWidget* stop_button;

  // Right Panel
  GtkWidget* server_host;
  GtkWidget* server_port;
  GtkWidget* username;
  GtkWidget* password;
  GtkWidget* method;
  GtkWidget* local_host;
  GtkWidget* local_port;
  GtkWidget* timeout;
  GtkWidget* autostart;
};

G_DEFINE_TYPE (YASSGtkWindow, yass_window, GTK_TYPE_APPLICATION_WINDOW)

static void
yass_window_init (YASSGtkWindow *win)
{
  GtkBuilder *builder;
  GMenuModel *menu;

  gtk_widget_init_template(GTK_WIDGET(win));

  builder = gtk_builder_new_from_resource("/it/gui/yass/menu.ui");
  menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON (win->gears), menu);
  g_object_unref(builder);
}

static void
yass_window_class_init (YASSGtkWindowClass *cls)
{
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (cls),
                                              "/it/gui/yass/yass_window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, gears);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, status_bar);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, start_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, stop_button);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, server_host);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, server_port);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, username);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, password);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, method);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, local_host);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, local_port);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, timeout);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (cls), YASSGtkWindow, autostart);
}

YASSGtkWindow *
yass_window_new (YASSGtkApp *app)
{
  return YASSGtk_WINDOW(g_object_new(yass_window_get_type(),
                                     "application", app, NULL));
}

} // extern "C"

YASSWindow::YASSWindow(GApplication *app)
    : impl_(yass_window_new (YASSGtk_APP (app))) {
#if 0
  gtk_window_set_position(GTK_WINDOW(impl_), GTK_WIN_POS_CENTER);
#endif
  gtk_window_set_resizable(GTK_WINDOW(impl_), false);
  gtk_window_set_icon_name(GTK_WINDOW(impl_), "yass");

  static YASSWindow* window = this;

  auto hide_callback = []() { window->OnClose(); };
  g_signal_connect(G_OBJECT(impl_), "hide",
                   G_CALLBACK(hide_callback), this);

  auto start_callback = []() { window->OnStartButtonClicked(); };

  g_signal_connect(G_OBJECT(impl_->start_button), "clicked",
                   G_CALLBACK(start_callback), nullptr);

  auto stop_callback = []() { window->OnStopButtonClicked(); };

  g_signal_connect(G_OBJECT(impl_->stop_button), "clicked",
                   G_CALLBACK(stop_callback), nullptr);

  gtk_widget_set_sensitive(GTK_WIDGET(impl_->stop_button), false);

  auto autostart_callback = []() { window->OnAutoStartClicked(); };

  g_signal_connect(G_OBJECT(impl_->autostart), "toggled",
                   G_CALLBACK(autostart_callback), nullptr);

  static const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  GtkComboBoxText *method = GTK_COMBO_BOX_TEXT(impl_->method);

  for (uint32_t i = 1; i < sizeof(method_names) / sizeof(method_names[0]);
       ++i) {
    gtk_combo_box_text_append_text(method, method_names[i]);
  }

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->autostart),
                              Utils::GetAutoStart());

  gtk_entry_set_visibility(GTK_ENTRY(impl_->password), false);

  GtkStatusbar *status_bar = GTK_STATUSBAR(impl_->status_bar);
  gtk_statusbar_remove_all(status_bar, 0);
  gtk_statusbar_push(status_bar, 0, "READY");

  LoadChanges();
}

YASSWindow::~YASSWindow() = default;

void YASSWindow::show() {
  gtk_widget_set_visible(GTK_WIDGET(impl_), true);
}

void YASSWindow::present() {
  gtk_window_present(GTK_WINDOW(impl_));
}

void YASSWindow::OnStartButtonClicked() {
  gtk_widget_set_sensitive(GTK_WIDGET(impl_->start_button), false);

  mApp->OnStart();
}

void YASSWindow::OnStopButtonClicked() {
  gtk_widget_set_sensitive(GTK_WIDGET(impl_->stop_button), false);
  mApp->OnStop();
}

void YASSWindow::OnAutoStartClicked() {
  Utils::EnableAutoStart(
      gtk_check_button_get_active(GTK_CHECK_BUTTON(impl_->autostart))
      );
}

std::string YASSWindow::GetServerHost() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->server_host));
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

std::string YASSWindow::GetTimeout() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->timeout));
}

std::string YASSWindow::GetStatusMessage() {
  if (mApp->GetState() != YASSApp::STARTED) {
    return mApp->GetStatus();
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time *
               NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time *
               NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  ss << mApp->GetStatus();
  ss << " tx rate: ";
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << " rx rate: ";
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return ss.str();
}

void YASSWindow::Started() {
  UpdateStatusBar();
  gtk_widget_set_sensitive(impl_->server_host, false);
  gtk_widget_set_sensitive(impl_->server_port, false);
  gtk_widget_set_sensitive(impl_->username, false);
  gtk_widget_set_sensitive(impl_->password, false);
  gtk_widget_set_sensitive(impl_->method, false);
  gtk_widget_set_sensitive(impl_->local_host, false);
  gtk_widget_set_sensitive(impl_->local_port, false);
  gtk_widget_set_sensitive(impl_->timeout, false);
  gtk_widget_set_sensitive(impl_->autostart, false);
  gtk_widget_set_sensitive(impl_->stop_button, true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();
  gtk_widget_set_sensitive(impl_->server_host, true);
  gtk_widget_set_sensitive(impl_->server_port, true);
  gtk_widget_set_sensitive(impl_->username, true);
  gtk_widget_set_sensitive(impl_->password, true);
  gtk_widget_set_sensitive(impl_->method, true);
  gtk_widget_set_sensitive(impl_->local_host, true);
  gtk_widget_set_sensitive(impl_->local_port, true);
  gtk_widget_set_sensitive(impl_->timeout, true);
  gtk_widget_set_sensitive(impl_->autostart, true);

  gtk_widget_set_sensitive(impl_->start_button, true);

  GtkDialog* dialog = GTK_DIALOG(
      gtk_message_dialog_new(GTK_WINDOW(impl_), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                             GTK_BUTTONS_CLOSE, "%s", mApp->GetStatus().c_str()));

  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
}

void YASSWindow::Stopped() {
  UpdateStatusBar();
  gtk_widget_set_sensitive(impl_->server_host, true);
  gtk_widget_set_sensitive(impl_->server_port, true);
  gtk_widget_set_sensitive(impl_->username, true);
  gtk_widget_set_sensitive(impl_->password, true);
  gtk_widget_set_sensitive(impl_->method, true);
  gtk_widget_set_sensitive(impl_->local_host, true);
  gtk_widget_set_sensitive(impl_->local_port, true);
  gtk_widget_set_sensitive(impl_->timeout, true);
  gtk_widget_set_sensitive(impl_->autostart, true);

  gtk_widget_set_sensitive(impl_->start_button, true);
}

void YASSWindow::LoadChanges() {
  auto server_host_str = absl::GetFlag(FLAGS_server_host);
  auto server_port_str = std::to_string(absl::GetFlag(FLAGS_server_port));
  auto username_str = absl::GetFlag(FLAGS_username);
  auto password_str = absl::GetFlag(FLAGS_password);
  int32_t cipher_method = absl::GetFlag(FLAGS_method).method;
  auto local_host_str = absl::GetFlag(FLAGS_local_host);
  auto local_port_str = std::to_string(absl::GetFlag(FLAGS_local_port));
  auto timeout_str = std::to_string(absl::GetFlag(FLAGS_connect_timeout));

  gtk_editable_set_text(GTK_EDITABLE(impl_->server_host), server_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->server_port), server_port_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->username), username_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->password), password_str.c_str());

  static const int method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  uint32_t i;
  for (i = 1; i < sizeof(method_ids) / sizeof(method_ids[0]); ++i) {
    if (cipher_method == method_ids[i])
      break;
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(impl_->method), i - 1);

  gtk_editable_set_text(GTK_EDITABLE(impl_->local_host), local_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->local_port), local_port_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->timeout), timeout_str.c_str());
}

void YASSWindow::UpdateStatusBar() {
  gtk_statusbar_remove_all(GTK_STATUSBAR(impl_->status_bar), 0);
  gtk_statusbar_push(GTK_STATUSBAR(impl_->status_bar), 0, GetStatusMessage().c_str());
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing ";
  mApp->Exit();
}
