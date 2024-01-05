// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */

#ifndef YASS_WINDOW_H
#define YASS_WINDOW_H

#include <gtk/gtk.h>

#include <string>

class YASSWindow {
 public:
  YASSWindow();
  ~YASSWindow();

 private:
  GtkWindow* impl_;

 public:
  void show();
  void present();

  // Left Panel
  GtkButton* start_button_;
  GtkButton* stop_button_;

  void OnStartButtonClicked();
  void OnStopButtonClicked();

  // Right Panel
  GtkEntry* server_host_;
  GtkEntry* server_sni_;
  GtkEntry* server_port_;
  GtkEntry* username_;
  GtkEntry* password_;
  GtkComboBoxText* method_;
  GtkEntry* local_host_;
  GtkEntry* local_port_;
  GtkEntry* timeout_;
  GtkCheckButton* autostart_;
  GtkCheckButton* systemproxy_;

  void OnAutoStartClicked();
  void OnSystemProxyClicked();

  GtkStatusbar* status_bar_;

 public:
  std::string GetServerHost();
  std::string GetServerSNI();
  std::string GetServerPort();
  std::string GetUsername();
  std::string GetPassword();
  std::string GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();
  std::string GetStatusMessage();

  void Started();
  void Stopped();
  void StartFailed();

  void LoadChanges();
  void UpdateStatusBar();

 private:
  void OnAbout();
  void OnOption();

 private:
  void OnClose();

  friend class YASSApp;

 private:
  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

#endif  // YASS_WINDOW_H
