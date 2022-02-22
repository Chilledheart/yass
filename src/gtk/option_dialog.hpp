// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2022 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include "glibmm/fake_typeid.hpp"

#include <gtkmm/dialog.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>

class OptionDialog : public Gtk::Dialog {
 public:
  explicit OptionDialog(const Glib::ustring& title, bool modal = false);

  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

 private:
  void LoadChanges();
  void OnSave();

  GtkEntry* connect_timeout_;
  GtkEntry* tcp_user_timeout_;
  GtkEntry* so_linger_timeout_;
  GtkEntry* so_snd_buffer_;
  GtkEntry* so_rcv_buffer_;

  GtkButton* okay_button_;
  GtkButton* cancel_button_;
};  // OptionDialog

#endif  // OPTION_DIALOG
