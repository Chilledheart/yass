// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_APP
#define YASS_WIN32_APP

#include "cli/cli_worker.hpp"

#include <windows.h>

class CYassFrame;
/// The main Application for YetAnotherShadowSocket
class CYassApp {
 public:
  CYassApp(HINSTANCE hInstance);
  ~CYassApp();

 private:
  const HINSTANCE m_hInstance;

 public:
  BOOL InitInstance();
  int ExitInstance();

  void OnStart(bool quiet = false);
  void OnStop(bool quiet = false);

  std::string GetStatus() const;
  enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };
  YASSState GetState() const { return state_; }

 private:
  void OnAppOption();
  static INT_PTR CALLBACK OnAppOptionMessage(HWND hDlg,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
  void OnAppAbout();
  static INT_PTR CALLBACK OnAppAboutMessage(HWND hDlg,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);

 private:
  void OnStarted(WPARAM w, LPARAM l);
  void OnStartFailed(WPARAM w, LPARAM l);
  void OnStopped(WPARAM w, LPARAM l);

 private:
  BOOL CheckFirstInstance();
  void LoadConfigFromDisk();
  void SaveConfigToDisk();

 private:
  YASSState state_;

  friend class CYassFrame;
  CYassFrame* frame_;

  Worker worker_;
  std::string error_msg_;
};

extern CYassApp* mApp;

#define WM_MYAPP_STARTED (WM_USER + 100)
#define WM_MYAPP_START_FAILED (WM_USER + 101)
#define WM_MYAPP_STOPPED (WM_USER + 102)

#endif  // YASS_WIN32_APP
