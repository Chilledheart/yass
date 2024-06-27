// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */
#ifndef OPTION_DIALOG_H
#define OPTION_DIALOG_H

#include <gtk/gtk.h>

#include <string>

extern "C" {
#define OPTION_DIALOG_TYPE (option_dialog_get_type())
#define OPTION_DIALOG(dialog) (G_TYPE_CHECK_INSTANCE_CAST((dialog), OPTION_DIALOG_TYPE, OptionGtkDialog))
G_DECLARE_FINAL_TYPE(OptionGtkDialog, option_dialog, OPTIONGtk, DIALOG, GtkDialog)

OptionGtkDialog* option_dialog_new(const gchar* title, GtkWindow* parent, GtkDialogFlags flags);
}

class OptionDialog {
 public:
  explicit OptionDialog(const std::string& title, GtkWindow* parent, bool modal = false);
  ~OptionDialog();

  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

  void run();

 private:
  void LoadChanges();
  bool OnSave();

 private:
  OptionGtkDialog* impl_;
};  // OptionDialog

#endif  // OPTION_DIALOG_H
