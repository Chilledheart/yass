// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "gui/yass.hpp"

#include <absl/flags/flag.h>
#include <stdexcept>
#include <string>
#ifdef __APPLE__
#include <pthread.h>
#endif

#include "core/logging.hpp"
#include "gui/panels.hpp"
#include "gui/utils.hpp"
#include "gui/yass_frame.hpp"

YASSApp* mApp = nullptr;

static const auto kMainFrameName = wxT("YetAnotherShadowSocket");

// this is a definition so can't be in a header
wxDEFINE_EVENT(MY_EVENT, wxCommandEvent);

wxIMPLEMENT_APP(YASSApp);

bool YASSApp::OnInit() {
  ::google::InitGoogleLogging("yass");
  ::FLAGS_stderrthreshold = true;
#ifndef NDEBUG
  ::FLAGS_logtostderr = true;
  ::FLAGS_logbuflevel = 0;
  ::FLAGS_v = 1;
#else
  ::FLAGS_logbuflevel = 1;
  ::FLAGS_v = 2;
#endif

#ifdef _WIN32
  HWND wnd = FindWindowW(nullptr, kMainFrameName);
  if (wnd) {
    wxMessageBox(wxT("Already exists!"), wxT("WndChecker"));
    return false;
  }
  if (Utils::SetProcessDpiAwareness()) {
    LOG(WARNING) << "SetProcessDpiAwareness applied";
  }
#endif
#ifdef __APPLE__
  /// documented in
  /// https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

  LOG(WARNING) << "Application starting";

  LoadConfigFromDisk();
  if (cipher_method_in_use == CRYPTO_INVALID) {
    wxMessageBox(wxT("Bad configuration: cipher method!"),
                 wxT("CypherChecker"));
    return false;
  }

  mApp = this;
  state_ = STOPPED;

  frame_ = new YASSFrame(kMainFrameName);
#if wxCHECK_VERSION(3, 1, 0)
  frame_->SetSize(frame_->FromDIP(wxSize(450, 390)));
#else
  frame_->SetSize(wxSize(450, 390));
#endif
  frame_->Centre();
  frame_->Show(true);
  frame_->UpdateStatus();
  SetTopWindow(frame_);

#if defined(__APPLE__) || defined(_WIN32)
  if (Utils::GetAutoStart()) {
    wxCommandEvent event;
    frame_->m_leftpanel->OnStart(event);
  }
#endif

  return true;
}

int YASSApp::OnExit() {
  LOG(WARNING) << "Application exiting";
  return wxApp::OnExit();
}

int YASSApp::OnRun() {
  int exitcode = wxApp::OnRun();
  LOG(INFO) << "Application is done with exitcode: " << exitcode;
  if (exitcode != 0) {
    return exitcode;
  }
  return 0;
}

std::string YASSApp::GetStatus() const {
  std::stringstream ss;
  if (state_ == STARTED) {
    ss << "Connected with conns: " << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    ss << "Failed to connect due to " << error_msg_.c_str();
  } else {
    ss << "Disconnected with " << worker_.GetRemoteEndpoint();
  }
  return ss.str();
};

void YASSApp::LoadConfigFromDisk() {
  config::ReadConfig();
}

void YASSApp::SaveConfigToDisk() {
  absl::SetFlag(&FLAGS_server_host, frame_->GetServerHost());
  absl::SetFlag(&FLAGS_server_port, Utils::Stoi(frame_->GetServerPort()));
  absl::SetFlag(&FLAGS_password, frame_->GetPassword());
  absl::SetFlag(&FLAGS_method, frame_->GetMethod());
  absl::SetFlag(&FLAGS_local_host, frame_->GetLocalHost());
  absl::SetFlag(&FLAGS_local_port, Utils::Stoi(frame_->GetLocalPort()));
  cipher_method_in_use = to_cipher_method(absl::GetFlag(FLAGS_method));
  absl::SetFlag(&FLAGS_timeout, Utils::Stoi(frame_->GetTimeout()));

  config::SaveConfig();
}

void YASSApp::OnStart(bool quiet) {
  state_ = STARTING;
  SaveConfigToDisk();
  worker_.Start(quiet);
}

void YASSApp::OnStarted(wxCommandEvent& WXUNUSED(event)) {
  state_ = STARTED;
  frame_->Started();
}

void YASSApp::OnStartFailed(wxCommandEvent& event) {
  state_ = START_FAILED;
  error_msg_ = event.GetString();
  frame_->StartFailed();
}

void YASSApp::OnStop(bool quiet) {
  state_ = STOPPING;
  worker_.Stop(quiet);
}

void YASSApp::OnStopped(wxCommandEvent& WXUNUSED(event)) {
  state_ = STOPPED;
  frame_->Stopped();
}

wxBEGIN_EVENT_TABLE(YASSApp, wxApp)
  EVT_COMMAND(ID_STARTED, MY_EVENT, YASSApp::OnStarted)
  EVT_COMMAND(ID_START_FAILED, MY_EVENT, YASSApp::OnStartFailed)
  EVT_COMMAND(ID_STOPPED, MY_EVENT, YASSApp::OnStopped)
wxEND_EVENT_TABLE()
