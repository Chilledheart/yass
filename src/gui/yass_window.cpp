// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#include "gui/yass_window.hpp"

#include <iomanip>
#include <sstream>

#include <absl/flags/flag.h>
#include <gtkmm/messagedialog.h>

#include "cli/socks5_connection_stats.hpp"
#include "core/utils.hpp"
#include "gui/option_dialog.hpp"
#include "gui/utils.hpp"
#include "gui/yass.hpp"

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
}  // namespace

YASSWindow::YASSWindow()
    : vbox_(false, 0),
      hbox_(false, 20),
      left_vbox_(false, 0),
      start_button_("Start"),
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
  // TODO set_icon_from_file();

  signal_hide().connect(sigc::mem_fun(*this, &YASSWindow::OnClose));

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

  gtk_box_pack_start(GTK_BOX(vbox_.gobj()), menubar, FALSE, FALSE, 0);

  start_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnStartButtonClicked));
  stop_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnStopButtonClicked));

  stop_button_.set_sensitive(false);

  start_button_.set_margin_top(30);
  start_button_.set_margin_bottom(30);
  stop_button_.set_margin_top(30);
  stop_button_.set_margin_bottom(30);

  left_vbox_.add(start_button_);
  left_vbox_.add(stop_button_);

  left_vbox_.set_margin_start(15);
  left_vbox_.set_margin_end(15);

  hbox_.add(left_vbox_);

  // right_panel_grid_.set_row_homogeneous(true);
  // right_panel_grid_.set_column_homogeneous(true);

  static const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  for (uint32_t i = 1; i < sizeof(method_names) / sizeof(method_names[0]);
       ++i) {
    method_.append(method_names[i]);
  }

  right_panel_grid_.attach(serverhost_label_, 0, 0);
  right_panel_grid_.attach(serverport_label_, 0, 1);
  right_panel_grid_.attach(password_label_, 0, 2);
  right_panel_grid_.attach(method_label_, 0, 3);
  right_panel_grid_.attach(localhost_label_, 0, 4);
  right_panel_grid_.attach(localport_label_, 0, 5);
  right_panel_grid_.attach(timeout_label_, 0, 6);
  right_panel_grid_.attach(autostart_label_, 0, 7);

  autostart_.signal_clicked().connect(
      sigc::mem_fun(*this, &YASSWindow::OnCheckedAutoStart));

  autostart_.set_active(Utils::GetAutoStart());

  password_.set_visibility(false);

  right_panel_grid_.attach(serverhost_, 1, 0);
  right_panel_grid_.attach(serverport_, 1, 1);
  right_panel_grid_.attach(password_, 1, 2);
  right_panel_grid_.attach(method_, 1, 3);
  right_panel_grid_.attach(localhost_, 1, 4);
  right_panel_grid_.attach(localport_, 1, 5);
  right_panel_grid_.attach(timeout_, 1, 6);
  right_panel_grid_.attach(autostart_, 1, 7);

  right_panel_grid_.set_margin_top(10);
  right_panel_grid_.set_margin_end(20);

  hbox_.add(right_panel_grid_);

  vbox_.pack_start(hbox_, true, false, 0);

  status_bar_.remove_all_messages();
  status_bar_.push("READY");
  vbox_.pack_start(status_bar_, true, false, 0);

  add(vbox_);

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
  return method_.get_active_text();
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
  method_.set_sensitive(false);
  localhost_.set_sensitive(false);
  localport_.set_sensitive(false);
  timeout_.set_sensitive(false);
  stop_button_.set_sensitive(true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();
  serverhost_.set_sensitive(true);
  serverport_.set_sensitive(true);
  password_.set_sensitive(true);
  method_.set_sensitive(true);
  localhost_.set_sensitive(true);
  localport_.set_sensitive(true);
  timeout_.set_sensitive(true);
  start_button_.set_sensitive(true);

  Gtk::MessageDialog alert_dialog(*this, mApp->GetStatus(), false,
                                  Gtk::MessageType::MESSAGE_WARNING,
                                  Gtk::ButtonsType::BUTTONS_OK, true);
  alert_dialog.run();
}

void YASSWindow::Stopped() {
  UpdateStatusBar();
  serverhost_.set_sensitive(true);
  serverport_.set_sensitive(true);
  password_.set_sensitive(true);
  method_.set_sensitive(true);
  localhost_.set_sensitive(true);
  localport_.set_sensitive(true);
  timeout_.set_sensitive(true);
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
  method_.set_active(i - 1);
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
  Gtk::MessageDialog about_dialog("This is Yet Another Shadow Socket", false,
                                  Gtk::MessageType::MESSAGE_INFO,
                                  Gtk::ButtonsType::BUTTONS_OK, true);
  about_dialog.run();
}

void YASSWindow::OnClose() {
  LOG(WARNING) << "Frame is closing ";
  mApp->Exit();
}
