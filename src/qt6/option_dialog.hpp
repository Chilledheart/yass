// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include <QDialog>

#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class OptionDialog : public QDialog {
  Q_OBJECT
 public:
  OptionDialog(QWidget* parent);

 private:
  void OnOkayButtonClicked();
  void OnCancelButtonClicked();

 private:
  void LoadChanges();
  bool OnSave();

 private:
  QCheckBox* tcp_keep_alive_;
  QLineEdit* tcp_keep_alive_cnt_;
  QLineEdit* tcp_keep_alive_idle_timeout_;
  QLineEdit* tcp_keep_alive_interval_;
  QCheckBox* enable_post_quantum_kyber_;
  QComboBox* tcp_congestion_algorithm_;
  std::vector<std::string> algorithms_;

  QPushButton* okay_button_;
  QPushButton* cancel_button_;
};

#endif  // OPTION_DIALOG
