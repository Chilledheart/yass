//
// yass.cpp
// ~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "yass.hpp"
#include "yass_frame.hpp"

#include <string>

#define UPDATE_STATS_MILLISECONDS 100

YASSApp *mApp;

// this is a definition so can't be in a header
wxDEFINE_EVENT(MY_EVENT, wxCommandEvent);

// clang-format off

wxBEGIN_EVENT_TABLE(YASSApp, wxApp)
  EVT_COMMAND(ID_STARTED, MY_EVENT, YASSApp::OnStarted)
  EVT_COMMAND(ID_START_FAILED, MY_EVENT, YASSApp::OnStartFailed)
  EVT_COMMAND(ID_STOPPED, MY_EVENT, YASSApp::OnStopped)
  EVT_TIMER(ID_UPDATE_STATS, YASSApp::OnUpdateStats)
wxEND_EVENT_TABLE()

    // clang-format on

    bool YASSApp::OnInit() {
  LoadConfigFromDisk();
  mApp = this;
  state_ = STOPPED;

  frame_ = new YASSFrame("Yet-Another-Shadow-Socket", wxPoint(50, 50),
                         wxSize(450, 340));
  frame_->Show(true);
  frame_->UpdateStatus();

  update_timer_ = new wxTimer(this, ID_UPDATE_STATS);
  return true;
}

std::string YASSApp::GetStatus() const {
  std::stringstream ss;
  if (state_ == STARTED) {
    ss << "Connected with " << worker_.GetRemoteEndpoint()
       << " with conns: " << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    ss << "Failed to connect with " << worker_.GetRemoteEndpoint() << " due to "
       << error_msg_.c_str();
  } else {
    ss << "Disconnected with " << worker_.GetRemoteEndpoint();
  }
  return ss.str();
};

void YASSApp::LoadConfigFromDisk() { ReadFromConfigfile(FLAGS_configfile); }

void YASSApp::SaveConfigToDisk() {
  FLAGS_server_host = frame_->GetServerHost();
  FLAGS_server_port = stoi(frame_->GetServerPort());
  FLAGS_password = frame_->GetPassword();
  FLAGS_method = frame_->GetMethod();
  FLAGS_local_host = frame_->GetLocalHost();
  FLAGS_local_port = stoi(frame_->GetLocalPort());
  cipher_method = cipher::to_cipher_method(FLAGS_method);

  SaveToConfigFile(FLAGS_configfile);
}

void YASSApp::OnStart() {
  SaveConfigToDisk();
  worker_.Start();
}

void YASSApp::OnStarted(wxCommandEvent &event) {
  state_ = STARTED;
  frame_->Started();
  update_timer_->Start(UPDATE_STATS_MILLISECONDS);
}

void YASSApp::OnStartFailed(wxCommandEvent &event) {
  state_ = START_FAILED;
  error_msg_ = event.GetString();
  frame_->StartFailed();
}

void YASSApp::OnStop() { worker_.Stop(); }

void YASSApp::OnStopped(wxCommandEvent &event) {
  state_ = STOPPED;
  frame_->Stopped();
  update_timer_->Stop();
}

void YASSApp::OnUpdateStats(wxTimerEvent &event) { frame_->UpdateStatus(); }

wxIMPLEMENT_APP(YASSApp);
