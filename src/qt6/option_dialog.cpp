// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#include "qt6/option_dialog.hpp"

#include <absl/flags/flag.h>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "net/network.hpp"

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

  auto enable_post_quantum_kyber_label = new QLabel(tr("Enables post-quantum key-agreements in TLS 1.3 connections"));
  auto tcp_congestion_algorithm_label = new QLabel(tr("TCP Congestion Algorithm"));

  grid->addWidget(tcp_keep_alive_label, 0, 0);
  grid->addWidget(tcp_keep_alive_cnt_label, 1, 0);
  grid->addWidget(tcp_keep_alive_idle_timeout_label, 2, 0);
  grid->addWidget(tcp_keep_alive_interval_label, 3, 0);
  grid->addWidget(enable_post_quantum_kyber_label, 4, 0);
  grid->addWidget(tcp_congestion_algorithm_label, 5, 0);

  tcp_keep_alive_ = new QCheckBox;
  tcp_keep_alive_cnt_ = new QLineEdit;
  tcp_keep_alive_cnt_->setValidator(new QIntValidator(0, INT32_MAX, this));
  tcp_keep_alive_idle_timeout_ = new QLineEdit;
  tcp_keep_alive_idle_timeout_->setValidator(new QIntValidator(0, INT32_MAX, this));
  tcp_keep_alive_interval_ = new QLineEdit;
  tcp_keep_alive_interval_->setValidator(new QIntValidator(0, INT32_MAX, this));
  enable_post_quantum_kyber_ = new QCheckBox;
  tcp_congestion_algorithm_ = new QComboBox;

  algorithms_ = net::GetTCPAvailableCongestionAlgorithms();

  for (const auto& algorithm : algorithms_) {
    tcp_congestion_algorithm_->addItem(algorithm.c_str());
  }

  grid->addWidget(tcp_keep_alive_, 0, 1);
  grid->addWidget(tcp_keep_alive_cnt_, 1, 1);
  grid->addWidget(tcp_keep_alive_idle_timeout_, 2, 1);
  grid->addWidget(tcp_keep_alive_interval_, 3, 1);
  grid->addWidget(enable_post_quantum_kyber_, 4, 1);
  grid->addWidget(tcp_congestion_algorithm_, 5, 1);

  okay_button_ = new QPushButton(tr("Okay"));
  connect(okay_button_, &QPushButton::clicked, this, &OptionDialog::OnOkayButtonClicked);

  cancel_button_ = new QPushButton(tr("Cancel"));
  connect(cancel_button_, &QPushButton::clicked, this, &OptionDialog::OnCancelButtonClicked);

  grid->addWidget(okay_button_, 6, 0);
  grid->addWidget(cancel_button_, 6, 1);

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

  auto algorithm = absl::GetFlag(FLAGS_tcp_congestion_algorithm);
  unsigned int i;
  for (i = 0; i < std::size(algorithms_); ++i) {
    if (algorithm == algorithms_[i])
      break;
  }

  // first is unset
  if (i == std::size(algorithms_)) {
    i = 0;
  }

  tcp_congestion_algorithm_->setCurrentIndex(i);
}

bool OptionDialog::OnSave() {
  auto tcp_keep_alive = tcp_keep_alive_->checkState() == Qt::CheckState::Checked;

  int tcp_keep_alive_cnt;
  if (!StringToInt(tcp_keep_alive_cnt_->text().toUtf8().data(), &tcp_keep_alive_cnt) || tcp_keep_alive_cnt < 0) {
    LOG(WARNING) << "invalid options: tcp_keep_alive_cnt";
    return false;
  }

  int tcp_keep_alive_idle_timeout;
  if (!StringToInt(tcp_keep_alive_idle_timeout_->text().toUtf8().data(), &tcp_keep_alive_idle_timeout) ||
      tcp_keep_alive_idle_timeout < 0) {
    LOG(WARNING) << "invalid options: tcp_keep_alive_idle_timeout";
    return false;
  }

  int tcp_keep_alive_interval;
  if (!StringToInt(tcp_keep_alive_interval_->text().toUtf8().data(), &tcp_keep_alive_interval) ||
      tcp_keep_alive_interval < 0) {
    LOG(WARNING) << "invalid options: tcp_keep_alive_interval";
    return false;
  }

  auto enable_post_quantum_kyber = enable_post_quantum_kyber_->checkState() == Qt::CheckState::Checked;

  absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, tcp_keep_alive_cnt);
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_idle_timeout);
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval);

  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, enable_post_quantum_kyber);

  int i = tcp_congestion_algorithm_->currentIndex();
  DCHECK_GE(i, 0);
  DCHECK_LE(i, (int)algorithms_.size());
  absl::SetFlag(&FLAGS_tcp_congestion_algorithm, algorithms_[i]);

  return true;
}
