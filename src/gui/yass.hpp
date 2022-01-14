// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef YASS_APP
#define YASS_APP

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include "cli/cli_worker.hpp"

class YASSFrame;
/// The main Application for YetAnotherShadowSocket
class YASSApp : public wxApp {
 public:
  // This is the very first function called for a newly created wxApp object,
  // it is used by the library to do the global initialization.
  bool Initialize(int& argc, wxChar** argv) override;
  // Called before OnRun(), this is a good place to do initialization -- if
  // anything fails, return false from here to prevent the program from
  // continuing. The command line is normally parsed here, call the base
  // class OnInit() to do it.
  bool OnInit() override;

  // Called before the first events are handled, called from within MainLoop()
  void OnLaunched() override;

  // This is only called if OnInit() returned true so it's a good place to do
  // any cleanup matching the initializations done there.
  int OnExit() override;

  // Used to by-pass wxWidgets' builtin parser by pass an empty argv
  //
  // called before wxCmdLineParser::Parse inside wxAppConsoleBase::OnInit
  void OnInitCmdLine(wxCmdLineParser& parser) override;

  void OnStart(bool quiet = false);
  void OnStop(bool quiet = false);

  std::string GetStatus() const;
  enum YASSState { STARTED, STARTING, START_FAILED, STOPPING, STOPPED };
  YASSState GetState() const { return state_; }

 private:
  void OnStarted(wxCommandEvent& event);
  void OnStartFailed(wxCommandEvent& event);
  void OnStopped(wxCommandEvent& event);

 private:
  void LoadConfigFromDisk();
  void SaveConfigToDisk();

 private:
  YASSState state_;

  friend class YASSFrame;
  YASSFrame* frame_;

  Worker worker_;
  wxString error_msg_;

  wxDECLARE_EVENT_TABLE();
};

extern YASSApp* mApp;

wxDECLARE_APP(YASSApp);

enum {
  ID_STARTED = 1,
  ID_START_FAILED = 2,
  ID_STOPPED = 3,
};

wxDECLARE_EVENT(MY_EVENT, wxCommandEvent);

#endif
