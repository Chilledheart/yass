// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#ifndef YASS_WINDOW_H
#define YASS_WINDOW_H

#include "glibmm/fake_typeid.hpp"

#include <gtkmm/applicationwindow.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcomboboxtext.h>
#include <gtk/gtkstatusbar.h>

class YASSWindow : public Gtk::Window {
 public:
  YASSWindow();
  ~YASSWindow() override;

  // Left Panel
  GtkButton* start_button_;
  GtkButton* stop_button_;

  void OnStartButtonClicked();
  void OnStopButtonClicked();

  // Right Panel
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
  GtkComboBoxText* method_;
  Gtk::Entry localhost_;
  Gtk::Entry localport_;
  Gtk::Entry timeout_;
  GtkCheckButton* autostart_;

  void OnCheckedAutoStart();

  GtkStatusbar* status_bar_;

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
