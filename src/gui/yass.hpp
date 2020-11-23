// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef YASS_APP
#define YASS_APP

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include "worker.hpp"

class YASSFrame;
/// The main Application for Yet-Another-Shadow-Socket
class YASSApp : public wxApp {
public:
  /// On Program Init
  bool OnInit() override;
  /// On Program Exit
  int OnExit() override;
  /// On Program Run
  int OnRun() override;

  void OnStart();
  void OnStop();

  std::string GetStatus() const;
  enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };
  YASSState GetState() const { return state_; }

private:
  void OnStarted(wxCommandEvent &event);
  void OnStartFailed(wxCommandEvent &event);
  void OnStopped(wxCommandEvent &event);

private:
  void LoadConfigFromDisk();
  void SaveConfigToDisk();

private:
  YASSState state_;

  YASSFrame *frame_;

  Worker worker_;
  wxString error_msg_;

  wxDECLARE_EVENT_TABLE();
};

extern YASSApp *mApp;

wxDECLARE_APP(YASSApp);

enum {
  ID_STARTED = 1,
  ID_START_FAILED = 2,
  ID_STOPPED = 3,
};

wxDECLARE_EVENT(MY_EVENT, wxCommandEvent);

#endif
