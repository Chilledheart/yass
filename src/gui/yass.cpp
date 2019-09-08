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

YASSApp *mApp;

bool YASSApp::OnInit() {
  LoadConfigFromDisk();
  mApp = this;
  state_ = STOPPED;

  frame_ = new YASSFrame("Yet-Another-Shadow-Socket", wxPoint(50, 50),
                         wxSize(450, 340));
  frame_->Show(true);
  frame_->UpdateStatus();
  return true;
}

std::string YASSApp::GetStatus() const {
  std::stringstream ss;
  if (state_ == STARTED) {
    ss << "Connected with " << worker_.GetRemoteEndpoint();
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
  state_ = STARTED;
  SaveConfigToDisk();
  worker_.Start();
  frame_->UpdateStatus();
}

void YASSApp::OnStop() {
  state_ = STOPPED;
  worker_.Stop();
  frame_->UpdateStatus();
}

wxIMPLEMENT_APP(YASSApp);
