// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */
#ifndef _YASS_QT6_YASS_HPP
#define _YASS_QT6_YASS_HPP

#include <QApplication>

#include "cli/cli_worker.hpp"

class QTimer;
class QTranslator;
class YASSWindow;
class YASSApp : public QApplication {
  Q_OBJECT
 public:
  YASSApp(int& argc, char** argv);

  bool Init();

 private slots:
  void OnIdle();

 public:
  void OnStart(bool quiet = false);
  void OnStop(bool quiet = false);

  std::string GetStatus() const;
  enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED, MAX_STATE };
  YASSState GetState() const { return state_; }

 signals:
  void OnStartedSignal();
  void OnStartFailedSignal(const std::string& error_msg);
  void OnStoppedSignal();

 private slots:
  void OnStarted();
  void OnStartFailed(const std::string& error_msg);
  void OnStopped();

 private:
  std::string SaveConfig();

 private:
  QTimer* idle_timer_;
  QTranslator* qt_translator_;
  QTranslator* my_translator_;

 private:
  YASSState state_ = STOPPED;

  friend class YASSWindow;
  YASSWindow* main_window_ = nullptr;

  std::unique_ptr<Worker> worker_;
  std::string error_msg_;
};

inline YASSApp* App() {
  return static_cast<YASSApp*>(qApp);
}

#endif  //  _YASS_QT6_YASS_HPP
