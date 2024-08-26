// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include <gtk/gtk.h>

#include <string>
#include <vector>

class OptionDialog {
 public:
  explicit OptionDialog(const std::string& title, GtkWindow* parent, bool modal = false);
  ~OptionDialog();

  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

  gint run();

 private:
  void LoadChanges();
  bool OnSave();

  GtkCheckButton* tcp_keep_alive_;
  GtkEntry* tcp_keep_alive_cnt_;
  GtkEntry* tcp_keep_alive_idle_timeout_;
  GtkEntry* tcp_keep_alive_interval_;
  GtkCheckButton* enable_post_quantum_kyber_;
  GtkComboBoxText* tcp_congestion_algorithm_;
  std::vector<std::string> algorithms_;

  GtkButton* okay_button_;
  GtkButton* cancel_button_;

 private:
  GtkDialog* impl_;
};  // OptionDialog

#endif  // OPTION_DIALOG
