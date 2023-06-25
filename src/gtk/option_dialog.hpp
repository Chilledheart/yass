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

  GtkCheckButton* tcp_keep_alive_;
  GtkEntry* tcp_keep_alive_cnt_;
  GtkEntry* tcp_keep_alive_idle_timeout_;
  GtkEntry* tcp_keep_alive_interval_;

  GtkButton* okay_button_;
  GtkButton* cancel_button_;

 private:
  GtkDialog* impl_;
};  // OptionDialog

#endif  // OPTION_DIALOG
