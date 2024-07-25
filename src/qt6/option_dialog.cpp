// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#include "qt6/option_dialog.hpp"

#include <absl/flags/flag.h>
#include <QCheckBox>
#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

OptionDialog::OptionDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("YASS Option"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QGridLayout* grid = new QGridLayout;
  grid->setContentsMargins(12, 12, 12, 12);
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(6);

  auto tcp_keep_alive_label = new QLabel(tr("TCP keep alive"));
  auto tcp_keep_alive_cnt_label = new QLabel(tr("The number of TCP keep-alive probes"));
  auto tcp_keep_alive_idle_timeout_label = new QLabel(tr("TCP keep alive after idle"));
  auto tcp_keep_alive_interval_label = new QLabel(tr("TCP keep alive interval"));

  auto enable_post_quantum_kyber_label = new QLabel(tr("Kyber post-quantum key agreement for TLS"));

  grid->addWidget(tcp_keep_alive_label, 0, 0);
  grid->addWidget(tcp_keep_alive_cnt_label, 1, 0);
  grid->addWidget(tcp_keep_alive_idle_timeout_label, 2, 0);
  grid->addWidget(tcp_keep_alive_interval_label, 3, 0);
  grid->addWidget(enable_post_quantum_kyber_label, 4, 0);

  tcp_keep_alive_ = new QCheckBox;
  tcp_keep_alive_cnt_ = new QLineEdit;
  tcp_keep_alive_cnt_->setValidator(new QIntValidator(0, INT32_MAX, this));
  tcp_keep_alive_idle_timeout_ = new QLineEdit;
  tcp_keep_alive_idle_timeout_->setValidator(new QIntValidator(0, INT32_MAX, this));
  tcp_keep_alive_interval_ = new QLineEdit;
  tcp_keep_alive_interval_->setValidator(new QIntValidator(0, INT32_MAX, this));
  enable_post_quantum_kyber_ = new QCheckBox;

  grid->addWidget(tcp_keep_alive_, 0, 1);
  grid->addWidget(tcp_keep_alive_cnt_, 1, 1);
  grid->addWidget(tcp_keep_alive_idle_timeout_, 2, 1);
  grid->addWidget(tcp_keep_alive_interval_, 3, 1);
  grid->addWidget(enable_post_quantum_kyber_, 4, 1);

  okay_button_ = new QPushButton(tr("Okay"));
  connect(okay_button_, &QPushButton::clicked, this, &OptionDialog::OnOkayButtonClicked);

  cancel_button_ = new QPushButton(tr("Cancel"));
  connect(cancel_button_, &QPushButton::clicked, this, &OptionDialog::OnCancelButtonClicked);

  grid->addWidget(okay_button_, 5, 0);
  grid->addWidget(cancel_button_, 5, 1);

  setLayout(grid);

  LoadChanges();
}

void OptionDialog::OnOkayButtonClicked() {
  if (!OnSave()) {
    return;
  }
  config::SaveConfig();
  accept();
}

void OptionDialog::OnCancelButtonClicked() {
  reject();
}

void OptionDialog::LoadChanges() {
  tcp_keep_alive_->setChecked(absl::GetFlag(FLAGS_tcp_keep_alive));
  auto tcp_keep_alive_cnt_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_cnt));
  tcp_keep_alive_cnt_->setText(QString::fromUtf8(tcp_keep_alive_cnt_str.c_str(), tcp_keep_alive_cnt_str.size()));
  auto tcp_keep_alive_idle_timeout_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout));
  tcp_keep_alive_idle_timeout_->setText(
      QString::fromUtf8(tcp_keep_alive_idle_timeout_str.c_str(), tcp_keep_alive_idle_timeout_str.size()));
  auto tcp_keep_alive_interval_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_interval));
  tcp_keep_alive_interval_->setText(
      QString::fromUtf8(tcp_keep_alive_interval_str.c_str(), tcp_keep_alive_interval_str.size()));

  enable_post_quantum_kyber_->setChecked(absl::GetFlag(FLAGS_enable_post_quantum_kyber));
}

bool OptionDialog::OnSave() {
  auto tcp_keep_alive = tcp_keep_alive_->checkState() == Qt::CheckState::Checked;
  auto tcp_keep_alive_cnt = StringToIntegerU(tcp_keep_alive_cnt_->text().toUtf8().data());
  auto tcp_keep_alive_idle_timeout = StringToIntegerU(tcp_keep_alive_idle_timeout_->text().toUtf8().data());
  auto tcp_keep_alive_interval = StringToIntegerU(tcp_keep_alive_interval_->text().toUtf8().data());

  auto enable_post_quantum_kyber = enable_post_quantum_kyber_->checkState() == Qt::CheckState::Checked;

  if (!tcp_keep_alive_cnt.has_value() || !tcp_keep_alive_idle_timeout.has_value() ||
      !tcp_keep_alive_interval.has_value()) {
    LOG(WARNING) << "invalid options";
    return false;
  }

  absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, tcp_keep_alive_cnt.value());
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_idle_timeout.value());
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval.value());

  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, enable_post_quantum_kyber);

  return true;
}
