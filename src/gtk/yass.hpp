// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#ifndef YASS_APP
#define YASS_APP

#include <memory>
#include <queue>

#include <absl/synchronization/mutex.h>
#include <gtk/gtk.h>

#include "cli/cli_worker.hpp"
#include "gtk/utils.hpp"

class YASSWindow;
/// The main Application for YetAnotherShadowSocket
class YASSApp {
 protected:
  YASSApp();

 public:
  ~YASSApp();

  static std::unique_ptr<YASSApp> create();

 private:
  GtkApplication *impl_;
  GSource *idle_source_;

 public:
  void OnActivate();

  Dispatcher dispatcher_;
  void OnIdle();

 public:
  // Glib event loop
  int ApplicationRun(int argc, char** argv);

  void Exit();

  void OnStart(bool quiet = false);
  void OnStop(bool quiet = false);

  std::string GetStatus() const;
  enum YASSState {
    STARTED,
    STARTING,
    START_FAILED,
    STOPPING,
    STOPPED,
    MAX_STATE
  };
  YASSState GetState() const { return state_; }

 private:
  void OnStarted();
  void OnStartFailed(const std::string& error_msg);
  void OnStopped();

  absl::Mutex dispatch_mutex_;
  std::queue<std::pair<YASSState, std::string>> dispatch_queue_;
  void OnDispatch();

 private:
  void SaveConfigToDisk();

 private:
  YASSState state_ = STOPPED;

  friend class YASSWindow;
  YASSWindow* main_window_ = nullptr;

  Worker worker_;
  std::string error_msg_;
};

extern YASSApp* mApp;

#endif
