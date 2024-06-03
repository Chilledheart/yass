// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#ifndef YASS_WINDOW_H
#define YASS_WINDOW_H

#include <string>

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QPushButton;
class QStatusBar;
class QLineEdit;
class YASSWindow : public QMainWindow {
  Q_OBJECT

 public:
  YASSWindow(QWidget* parent = nullptr);

  void moveToCenter();
  void showWindow();

  friend class YASSApp;

 public slots:
  void OnStartButtonClicked();
  void OnStopButtonClicked();

  void OnAutoStartClicked();
  void OnSystemProxyClicked();

 private:
  // Left Panel
  QPushButton* start_button_;
  QPushButton* stop_button_;

  // Right Panel
  QLineEdit* server_host_;
  QLineEdit* server_sni_;
  QLineEdit* server_port_;
  QLineEdit* username_;
  QLineEdit* password_;
  QComboBox* method_;
  QLineEdit* local_host_;
  QLineEdit* local_port_;
  QLineEdit* doh_url_;
  QLineEdit* dot_host_;
  QLineEdit* timeout_;
  QCheckBox* autostart_;
  QCheckBox* systemproxy_;

  QStatusBar* status_bar_;
  std::string last_status_msg_;

 public:
  std::string GetServerHost();
  std::string GetServerSNI();
  std::string GetServerPort();
  std::string GetUsername();
  std::string GetPassword();
  std::string GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetDoHUrl();
  std::string GetDoTHost();
  std::string GetTimeout();
  std::string GetStatusMessage();

  void Started();
  void StartFailed();
  void Stopped();

  void LoadChanges();
  void UpdateStatusBar();

 private slots:
  void OnAbout();
  void OnOption();

 private:
  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

#endif  // YASS_WINDOW_H
