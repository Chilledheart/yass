// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#include "gtk/yass_window.hpp"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>

#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkgrid.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmessagedialog.h>

#include "cli/socks5_connection_stats.hpp"
#include "core/utils.hpp"
#include "gtk/option_dialog.hpp"
#include "gtk/utils.hpp"
#include "gtk/yass.hpp"
#include "version.h"

static const char* kMainFrameName = "YetAnotherShadowSocket";

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

namespace {
template <typename Handler, void (Handler::*HandlerFunc)()>
struct GtkHandlerProxy {
  static void Handle(gpointer p) { (static_cast<Handler*>(p)->*HandlerFunc)(); }
};

template <typename T>
std::unique_ptr<T[], decltype(&g_free)> make_unique_ptr_gfree(T* p) {
  return std::unique_ptr<T[], decltype(&g_free)>(p, &g_free);
}
}  // namespace

YASSWindow::YASSWindow()
    : start_button_("Start"),
      stop_button_("Stop"),
      serverhost_label_("Server Host"),
      serverport_label_("Server Port"),
      password_label_("Password"),
      method_label_("Cipher/Method"),
      localhost_label_("Local Host"),
      localport_label_("Local Port"),
      timeout_label_("Timeout"),
      autostart_label_("Auto Start") {
  set_title(kMainFrameName);
  set_default_size(450, 390);
  set_position(Gtk::WIN_POS_CENTER);
  set_resizable(false);
  set_icon_name("yass");

  signal_hide().connect(sigc::mem_fun(*this, &YASSWindow::OnClose));

  GtkBox *vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  GtkBox *hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20));
  GtkBox *left_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  GtkGrid* right_panel_grid = GTK_GRID(gtk_grid_new());

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
  file_menu_item = gtk_menu_item_new_with_label("File");
  option_menu_item = gtk_menu_item_new_with_label("Option...");
  exit_menu_item = gtk_menu_item_new_with_label("Exit");

  sep = gtk_separator_menu_item_new();

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), option_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), sep);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

  auto option_callback =
      &GtkHandlerProxy<YASSWindow, &YASSWindow::OnOption>::Handle;
  g_signal_connect(G_OBJECT(option_menu_item), "activate",
                   G_CALLBACK(option_callback), nullptr);

  auto exit_callback =
      &GtkHandlerProxy<YASSWindow, &YASSWindow::OnClose>::Handle;
  g_signal_connect(G_OBJECT(exit_menu_item), "activate",
                   G_CALLBACK(exit_callback), nullptr);

  help_menu = gtk_menu_new();
  help_menu_item = gtk_menu_item_new_with_label("Help");
  about_menu_item = gtk_menu_item_new_with_label("About...");

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);

  auto about_callback =
      &GtkHandlerProxy<YASSWindow, &YASSWindow::OnAbout>::Handle;
  g_signal_connect(G_OBJECT(about_menu_item), "activate",
                   G_CALLBACK(about_callback), this);

  gtk_box_pack_start(vbox, menubar, FALSE, FALSE, 0);

  start_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnStartButtonClicked));
  stop_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnStopButtonClicked));

  stop_button_.set_sensitive(false);

  start_button_.set_margin_top(30);
  start_button_.set_margin_bottom(30);
  stop_button_.set_margin_top(30);
  stop_button_.set_margin_bottom(30);

  gtk_container_add(GTK_CONTAINER(left_box), GTK_WIDGET(start_button_.gobj()));
  gtk_container_add(GTK_CONTAINER(left_box), GTK_WIDGET(stop_button_.gobj()));

  gtk_widget_set_margin_start(GTK_WIDGET(left_box), 15);
  gtk_widget_set_margin_end(GTK_WIDGET(left_box), 15);

  gtk_container_add(GTK_CONTAINER(hbox), GTK_WIDGET(left_box));

  static const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  method_ = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());

  for (uint32_t i = 1; i < sizeof(method_names) / sizeof(method_names[0]);
       ++i) {
    gtk_combo_box_text_append_text(method_, method_names[i]);
  }

  gtk_grid_attach(right_panel_grid, GTK_WIDGET(serverhost_label_.gobj()), 0, 0, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(serverport_label_.gobj()), 0, 1, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(password_label_.gobj()), 0, 2, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(method_label_.gobj()), 0, 3, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(localhost_label_.gobj()), 0, 4, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(localport_label_.gobj()), 0, 5, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(timeout_label_.gobj()), 0, 6, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(autostart_label_.gobj()), 0, 7, 1, 1);

  autostart_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnCheckedAutoStart));

  autostart_.set_active(Utils::GetAutoStart());

  password_.set_visibility(false);

  gtk_grid_attach(right_panel_grid, GTK_WIDGET(serverhost_.gobj()), 1, 0, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(serverport_.gobj()), 1, 1, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(password_.gobj()), 1, 2, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(method_), 1, 3, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(localhost_.gobj()), 1, 4, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(localport_.gobj()), 1, 5, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(timeout_.gobj()), 1, 6, 1, 1);
  gtk_grid_attach(right_panel_grid, GTK_WIDGET(autostart_.gobj()), 1, 7, 1, 1);

  gtk_widget_set_margin_start(GTK_WIDGET(right_panel_grid), 10);
  gtk_widget_set_margin_end(GTK_WIDGET(right_panel_grid), 20);

  gtk_container_add(GTK_CONTAINER(hbox), GTK_WIDGET(right_panel_grid));

  gtk_box_pack_start(vbox, GTK_WIDGET(hbox), true, false, 0);

  status_bar_.remove_all_messages();
  status_bar_.push("READY");

  gtk_box_pack_start(vbox, status_bar_.Gtk::Widget::gobj(), true, false, 0);

  Utils::DisableGtkRTTI(this);

  gtk_container_add(Gtk::Container::gobj(), GTK_WIDGET(vbox));

  LoadChanges();

  show_all_children();
}

YASSWindow::~YASSWindow() = default;

void YASSWindow::OnStartButtonClicked() {
  start_button_.set_sensitive(false);
  mApp->OnStart();
}

void YASSWindow::OnStopButtonClicked() {
  stop_button_.set_sensitive(false);
  mApp->OnStop();
}

void YASSWindow::OnCheckedAutoStart() {
  Utils::EnableAutoStart(autostart_.get_active());
}

std::string YASSWindow::GetServerHost() {
  return serverhost_.get_text();
}

std::string YASSWindow::GetServerPort() {
  return serverport_.get_text();
}

std::string YASSWindow::GetPassword() {
  return password_.get_text();
}

std::string YASSWindow::GetMethod() {
  gchar* active_method = gtk_combo_box_text_get_active_text(method_);

  return make_unique_ptr_gfree(active_method).get();
}

std::string YASSWindow::GetLocalHost() {
  return localhost_.get_text();
}

std::string YASSWindow::GetLocalPort() {
  return localport_.get_text();
}

std::string YASSWindow::GetTimeout() {
  return timeout_.get_text();
}

void YASSWindow::Started() {
  UpdateStatusBar();
  serverhost_.set_sensitive(false);
  serverport_.set_sensitive(false);
  password_.set_sensitive(false);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), false);
  localhost_.set_sensitive(false);
  localport_.set_sensitive(false);
  timeout_.set_sensitive(false);
  autostart_.set_sensitive(false);
  stop_button_.set_sensitive(true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();
  serverhost_.set_sensitive(true);
  serverport_.set_sensitive(true);
  password_.set_sensitive(true);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), true);
  localhost_.set_sensitive(true);
  localport_.set_sensitive(true);
  timeout_.set_sensitive(true);
  autostart_.set_sensitive(true);
  start_button_.set_sensitive(true);

  GtkDialog* alert_dialog = GTK_DIALOG(gtk_message_dialog_new(
      Gtk::Window::gobj(), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
      GTK_BUTTONS_OK, "%s", mApp->GetStatus().c_str()));

  gtk_dialog_run(GTK_DIALOG(alert_dialog));
  gtk_widget_destroy(GTK_WIDGET(alert_dialog));
}

void YASSWindow::Stopped() {
  UpdateStatusBar();
  serverhost_.set_sensitive(true);
  serverport_.set_sensitive(true);
  password_.set_sensitive(true);
  gtk_widget_set_sensitive(GTK_WIDGET(method_), true);
  localhost_.set_sensitive(true);
  localport_.set_sensitive(true);
  timeout_.set_sensitive(true);
  autostart_.set_sensitive(true);
  start_button_.set_sensitive(true);
}

void YASSWindow::LoadChanges() {
  serverhost_.set_text(absl::GetFlag(FLAGS_server_host));
  serverport_.set_text(std::to_string(absl::GetFlag(FLAGS_server_port)));
  password_.set_text(absl::GetFlag(FLAGS_password));

  static const int method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  int32_t cipher_method = absl::GetFlag(FLAGS_cipher_method);
  uint32_t i;
  for (i = 1; i < sizeof(method_ids) / sizeof(method_ids[0]); ++i) {
    if (cipher_method == method_ids[i])
      break;
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(method_), i - 1);

  localhost_.set_text(absl::GetFlag(FLAGS_local_host));
  localport_.set_text(std::to_string(absl::GetFlag(FLAGS_local_port)));
  timeout_.set_text(std::to_string(absl::GetFlag(FLAGS_connect_timeout)));
}

void YASSWindow::UpdateStatusBar() {
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = total_rx_bytes;
    uint64_t tx_bytes = total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time *
               NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time *
               NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::stringstream ss;
  ss << mApp->GetStatus();
  ss << " tx rate: ";
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << " rx rate: ";
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  status_bar_.remove_all_messages();
  status_bar_.push(ss.str());
}

void YASSWindow::OnOption() {
  OptionDialog option_dialog("YASS Option", true);

  int ret = option_dialog.run();
  if (ret == Gtk::RESPONSE_ACCEPT) {
    mApp->SaveConfigToDisk();
  }
}

void YASSWindow::OnAbout() {
  GtkAboutDialog* about_dialog = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
  const char* artists[] = {"macosicons.com", nullptr};
  gtk_about_dialog_set_artists(about_dialog, artists);
  const char* authors[] = {YASS_APP_COMPANY_NAME, nullptr};
  gtk_about_dialog_set_authors(about_dialog, authors);
  gtk_about_dialog_set_comments(about_dialog, "Last Change: " YASS_APP_LAST_CHANGE);
  gtk_about_dialog_set_copyright(about_dialog, YASS_APP_COPYRIGHT);
  gtk_about_dialog_set_license_type(about_dialog, GTK_LICENSE_GPL_2_0);
  gtk_about_dialog_set_logo_icon_name(about_dialog, "yass");
  gtk_about_dialog_set_program_name(about_dialog, YASS_APP_PRODUCT_NAME);
  gtk_about_dialog_set_version(about_dialog, YASS_APP_PRODUCT_VERSION);
  gtk_about_dialog_set_website(about_dialog, YASS_APP_WEBSITE);
  gtk_about_dialog_set_website_label(about_dialog, "official-site");
  gtk_dialog_run(GTK_DIALOG(about_dialog));
  gtk_widget_destroy(GTK_WIDGET(about_dialog));
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing ";

  mApp->Exit();

  close();
}
