// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#ifndef YASS_WINDOW_H
#define YASS_WINDOW_H

#include <gtk/gtk.h>

#include <string>

#include "gtk4/yass.hpp"

extern "C" {
#define YASS_WINDOW_TYPE (yass_window_get_type ())
G_DECLARE_FINAL_TYPE (YASSGtkWindow, yass_window, YASSGtk, WINDOW, GtkApplicationWindow)

YASSGtkWindow       *yass_window_new          (YASSGtkApp *app);
}

class YASSWindow {
 public:
  YASSWindow(GApplication *app);
  ~YASSWindow();

  YASSGtkWindow* impl() {
    return impl_;
  }
 private:
  YASSGtkWindow* impl_;
  std::string last_status_msg_;

 public:
  void show();
  void present();

  void OnStartButtonClicked();
  void OnStopButtonClicked();
  void OnAutoStartClicked();
  void OnSystemProxyClicked();

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
