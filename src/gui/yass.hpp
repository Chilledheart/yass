// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#ifndef YASS_APP
#define YASS_APP

#include <queue>

#include <absl/synchronization/mutex.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/application.h>

#include "cli/cli_worker.hpp"

class YASSWindow;
/// The main Application for YetAnotherShadowSocket
class YASSApp : public Gtk::Application {
 protected:
  YASSApp();

 public:
  ~YASSApp() override;

  static Glib::RefPtr<YASSApp> create();

 protected:
  // The signal_startup() signal is emitted on the primary instance immediately
  // after registration. See g_application_register().
  void on_startup() override;

  // To handle these two cases, we override signal_activate()'s default handler,
  // which gets called when the application is launched without commandline
  // arguments, and signal_open()'s default handler, which gets called when the
  // application is launched with commandline arguments.
  void on_activate() override;

  // returning false in the signal handler causes remove the signal handler
  bool OnIdle();

  sigc::connection idle_connection_;

 public:
  // Glib event loop
  int ApplicationRun();

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

  guint worker_signals_[MAX_STATE];
  absl::Mutex dispatch_mutex_;
  std::queue<std::pair<YASSState, std::string>> dispatch_queue_;
  void OnDispatch();

  Glib::Dispatcher dispatcher_;

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
