// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef YASS_WINDOW_H
#define YASS_WINDOW_H

#include <gtkmm/applicationwindow.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/statusbar.h>

class YASSWindow : public Gtk::Window {
 public:
  YASSWindow();
  ~YASSWindow() override;

  Gtk::VBox vbox_;

  Gtk::HBox hbox_;
  // Left Panel
  Gtk::VBox left_vbox_;
  Gtk::Button start_button_;
  Gtk::Button stop_button_;

  void OnStartButtonClicked();
  void OnStopButtonClicked();

  // Right Panel
  Gtk::Grid right_panel_grid_;

  Gtk::Label serverhost_label_;
  Gtk::Label serverport_label_;
  Gtk::Label password_label_;
  Gtk::Label method_label_;
  Gtk::Label localhost_label_;
  Gtk::Label localport_label_;
  Gtk::Label timeout_label_;
  Gtk::Label autostart_label_;

  Gtk::Entry serverhost_;
  Gtk::Entry serverport_;
  Gtk::Entry password_;
  Gtk::ComboBoxText method_;
  Gtk::Entry localhost_;
  Gtk::Entry localport_;
  Gtk::Entry timeout_;
  Gtk::CheckButton autostart_;

  void OnCheckedAutoStart();

  Gtk::Statusbar status_bar_;

 public:
  std::string GetServerHost();
  std::string GetServerPort();
  std::string GetPassword();
  std::string GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();

  void Started();
  void Stopped();
  void StartFailed();

  void LoadChanges();
  void UpdateStatusBar();

 private:
  void OnAbout();
  void OnOption();

 private:
  // TODO exit when main window close
  void OnClose();

  friend class YASSApp;

 private:
  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

#endif  // YASS_WINDOW_H
