// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2022 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include <gtk/gtk.h>

#include <string>

class OptionDialog {
 public:
  explicit OptionDialog(const std::string& title,
                        GtkWindow* parent,
                        bool modal = false);
  ~OptionDialog();

  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

  gint run();

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

 private:
  GtkDialog* impl_;
};  // OptionDialog

#endif  // OPTION_DIALOG
