// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2022 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include "glibmm/fake_typeid.hpp"

#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>

class OptionDialog : public Gtk::Dialog {
 public:
  explicit OptionDialog(const Glib::ustring& title, bool modal = false);

  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

 private:
  void LoadChanges();
  void OnSave();

  Gtk::Grid grid_;

  Gtk::Label connecttimeout_label_;
  Gtk::Label tcpusertimeout_label_;
  Gtk::Label lingertimeout_label_;
  Gtk::Label sendbuffer_label_;
  Gtk::Label recvbuffer_label_;

  Gtk::Entry connecttimeout_;
  Gtk::Entry tcpusertimeout_;
  Gtk::Entry lingertimeout_;
  Gtk::Entry sendbuffer_;
  Gtk::Entry recvbuffer_;

  Gtk::Button okay_button_;
  Gtk::Button cancel_button_;
};  // OptionDialog

#endif  // OPTION_DIALOG
