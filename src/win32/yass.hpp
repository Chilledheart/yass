// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_APP
#define YASS_WIN32_APP
#include <afxext.h>  // MFC extensions (including VB)
#include <afxtempl.h>
#include <afxwin.h>  // MFC core and standard components

#include "cli/cli_worker.hpp"

class CYassFrame;
/// The main Application for YetAnotherShadowSocket
/// https://docs.microsoft.com/en-us/cpp/mfc/reference/cwinapp-class?view=msvc-170
class CYassApp : public CWinApp {
 public:
  CYassApp();
  ~CYassApp();
  // Application initialization is conceptually divided into two sections:
  // one-time application initialization that is done the first time
  // the program runs, and instance initialization that runs each time
  // a copy of the program runs, including the first time.
  // The framework's implementation of WinMain calls this function.
  BOOL InitInstance() override;

  // Called by the framework from within the Run member function
  // to exit this instance of the application.
  int ExitInstance() override;

  void OnStart(bool quiet = false);
  void OnStop(bool quiet = false);

  std::string GetStatus() const;
  enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };
  YASSState GetState() const { return state_; }

 private:
  afx_msg void OnStarted(WPARAM w, LPARAM l);
  afx_msg void OnStartFailed(WPARAM w, LPARAM l);
  afx_msg void OnStopped(WPARAM w, LPARAM l);

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

  DECLARE_MESSAGE_MAP();
};

extern CYassApp* mApp;

#define WM_MYAPP_STARTED (WM_USER + 100)
#define WM_MYAPP_START_FAILED (WM_USER + 101)
#define WM_MYAPP_STOPPED (WM_USER + 102)

#endif  // YASS_WIN32_APP
