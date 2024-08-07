// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2024 Chilledheart  */

#include "gtk4/yass_window.hpp"

#include <sstream>

#include <absl/flags/flag.h>
#include <glib/gi18n.h>

#include "cli/cli_connection_stats.hpp"
#include "core/utils.hpp"
#include "feature.h"
#include "freedesktop/utils.hpp"
#include "gtk/utils.hpp"
#include "gtk4/option_dialog.hpp"
#include "gtk4/yass.hpp"
#include "gui_variant.h"
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
  GtkWidget* limit_rate;
  GtkWidget* timeout;
  GtkWidget* autostart;
  GtkWidget* systemproxy;
};

G_DEFINE_TYPE(YASSGtkWindow, yass_window, GTK_TYPE_APPLICATION_WINDOW)

static void yass_window_init(YASSGtkWindow* win) {
  GtkBuilder* builder;
  GMenuModel* menu;

  gtk_widget_init_template(GTK_WIDGET(win));

  builder = gtk_builder_new_from_resource("/io/github/chilledheart/yass/menu.ui");
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
  gtk_widget_unparent(window->limit_rate);
  gtk_widget_unparent(window->timeout);
  gtk_widget_unparent(window->autostart);
  gtk_widget_unparent(window->systemproxy);
#endif
  G_OBJECT_CLASS(yass_window_parent_class)->dispose(object);
}

static void yass_window_class_init(YASSGtkWindowClass* cls) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(cls), "/io/github/chilledheart/yass/yass_window.ui");
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, limit_rate);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, timeout);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, autostart);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), YASSGtkWindow, systemproxy);

  G_OBJECT_CLASS(cls)->dispose = yass_window_dispose;
}

YASSGtkWindow* yass_window_new(YASSGtkApp* app) {
  auto window = YASSGtk_WINDOW(g_object_new(yass_window_get_type(), "application", app, nullptr));
  gtk_window_set_resizable(GTK_WINDOW(window), false);
  gtk_window_set_icon_name(GTK_WINDOW(window), "io.github.chilledheart.yass");
  return window;
}

}  // extern "C"

YASSWindow::YASSWindow(GApplication* app) : app_(app), impl_(yass_window_new(YASSGtk_APP(app))) {
  auto close_callback = [](GtkWindow* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->close_requested_ = true;
    window->OnClose();
  };
  g_signal_connect(G_OBJECT(impl_), "close-request", G_CALLBACK(*close_callback), this);

  auto start_callback = [](GtkButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnStartButtonClicked();
  };

  g_signal_connect(G_OBJECT(impl_->start_button), "clicked", G_CALLBACK(*start_callback), this);

  auto stop_callback = [](GtkButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnStopButtonClicked();
  };

  g_signal_connect(G_OBJECT(impl_->stop_button), "clicked", G_CALLBACK(*stop_callback), this);

  gtk_widget_set_sensitive(GTK_WIDGET(impl_->stop_button), false);

  auto autostart_callback = [](GtkToggleButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnAutoStartClicked();
  };

  g_signal_connect(G_OBJECT(impl_->autostart), "toggled", G_CALLBACK(*autostart_callback), this);

  auto systemproxy_callback = [](GtkToggleButton* self, gpointer pointer) {
    YASSWindow* window = (YASSWindow*)pointer;
    window->OnSystemProxyClicked();
  };

  g_signal_connect(G_OBJECT(impl_->systemproxy), "toggled", G_CALLBACK(*systemproxy_callback), this);

  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkComboBoxText* method = GTK_COMBO_BOX_TEXT(impl_->method);
  for (const char* method_name : method_names) {
    gtk_combo_box_text_append_text(method, method_name);
  }
  G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->autostart), Utils::GetAutoStart());

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->systemproxy), Utils::GetSystemProxy());

  gtk_entry_set_visibility(GTK_ENTRY(impl_->password), false);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkStatusbar* status_bar = GTK_STATUSBAR(impl_->status_bar);
  gtk_statusbar_remove_all(status_bar, 0);
  gtk_statusbar_push(status_bar, 0, _("READY"));
  G_GNUC_END_IGNORE_DEPRECATIONS

  LoadChanges();
}

YASSWindow::~YASSWindow() {
  if (!close_requested_) {
    gtk_window_destroy(GTK_WINDOW(impl_));
  }
}

void YASSWindow::show() {
  gtk_widget_set_visible(GTK_WIDGET(impl_), true);
}

void YASSWindow::present() {
  gtk_window_present(GTK_WINDOW(impl_));
}

void YASSWindow::close() {
  if (about_dialog_) {
    gtk_window_close(GTK_WINDOW(about_dialog_));
  }
  if (option_dialog_) {
    option_dialog_->OnCancelButtonClicked();
  }
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
  gtk_widget_set_sensitive(impl_->limit_rate, false);
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

void YASSWindow::OnAbout() {
  if (!about_dialog_) {
    LOG(INFO) << "About Dialog new";
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
    gtk_about_dialog_set_logo_icon_name(about_dialog, "io.github.chilledheart.yass");
    gtk_about_dialog_set_program_name(about_dialog, YASS_APP_PRODUCT_NAME);
    gtk_about_dialog_set_version(about_dialog, YASS_APP_PRODUCT_VERSION);
    gtk_about_dialog_set_website(about_dialog, YASS_APP_WEBSITE);
    gtk_about_dialog_set_website_label(about_dialog, _("official-site"));

    auto close_callback = [](GtkWindow* self, gpointer pointer) {
      YASSWindow* window = (YASSWindow*)pointer;
      window->OnAboutDialogClose();
    };
    g_signal_connect(G_OBJECT(about_dialog), "close-request", G_CALLBACK(*close_callback), this);

    about_dialog_ = about_dialog;
  }

  gtk_window_present(GTK_WINDOW(about_dialog_));
}

void YASSWindow::OnOption() {
  if (!option_dialog_) {
    LOG(INFO) << "Option Dialog new";
    auto dialog = new OptionDialog(_("YASS Option"), GTK_WINDOW(impl_), true);

    auto response_callback = [](GtkDialog* self, gint response_id, gpointer pointer) {
      YASSWindow* window = (YASSWindow*)pointer;
      window->OnOptionDialogClose();
    };

    g_signal_connect(dialog->impl_, "response", G_CALLBACK(*response_callback), this);

    option_dialog_ = dialog;
  }
  option_dialog_->run();
}

void YASSWindow::OnAboutDialogClose() {
  DCHECK(about_dialog_);
  LOG(INFO) << "About Dialog closed";
  gtk_window_destroy(GTK_WINDOW(about_dialog_));
  about_dialog_ = nullptr;
}

void YASSWindow::OnOptionDialogClose() {
  DCHECK(option_dialog_);
  LOG(INFO) << "Option Dialog closed";

  delete option_dialog_;
  option_dialog_ = nullptr;
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
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gchar* active_method = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(impl_->method));
  G_GNUC_END_IGNORE_DEPRECATIONS

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

std::string YASSWindow::GetLimitRate() {
  return gtk_editable_get_text(GTK_EDITABLE(impl_->limit_rate));
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
  gtk_widget_set_sensitive(impl_->limit_rate, true);
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

  g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), this);
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
  gtk_widget_set_sensitive(impl_->limit_rate, true);
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
  std::string limit_rate_str = absl::GetFlag(FLAGS_limit_rate);
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

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_combo_box_set_active(GTK_COMBO_BOX(impl_->method), i);
  G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_editable_set_text(GTK_EDITABLE(impl_->local_host), local_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->local_port), local_port_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->doh_url), doh_url_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->dot_host), dot_host_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->limit_rate), limit_rate_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->timeout), timeout_str.c_str());
}

void YASSWindow::UpdateStatusBar() {
  std::string status_msg = GetStatusMessage();
  if (last_status_msg_ == status_msg) {
    return;
  }
  last_status_msg_ = status_msg;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkStatusbar* status_bar = GTK_STATUSBAR(impl_->status_bar);
  gtk_statusbar_remove_all(status_bar, 0);
  gtk_statusbar_push(status_bar, 0, last_status_msg_.c_str());
  G_GNUC_END_IGNORE_DEPRECATIONS
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing";
  mApp->Exit();
}
