// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "gui/yass.hpp"

#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <wx/cmdline.h>
#include <wx/stdpaths.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "gui/panels.hpp"
#include "gui/yass_frame.hpp"
#include "gui/yass_logging.hpp"

YASSApp* mApp = nullptr;

static const auto kMainFrameName = wxT("YetAnotherShadowSocket");

// this is a definition so can't be in a header
wxDEFINE_EVENT(MY_EVENT, wxCommandEvent);

wxIMPLEMENT_APP(YASSApp);

YASSApp::~YASSApp() {
  delete wxLog::SetActiveTarget(nullptr);
}

bool YASSApp::Initialize(int& wxapp_argc, wxChar** wxapp_argv) {
  if (!wxApp::Initialize(wxapp_argc, wxapp_argv))
    return false;

  wxString appPath = wxStandardPaths::Get().GetExecutablePath();
  absl::InitializeSymbolizer(appPath.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  return true;
}

bool YASSApp::OnInit() {
  absl::ParseCommandLine(argc, argv.operator char* *());

  if (!wxApp::OnInit())
    return false;

  auto cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
  }

  LoadConfigFromDisk();
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  logger_ = new YASSLog;
  delete wxLog::SetActiveTarget(logger_);

  LOG(WARNING) << "Application starting";

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

  return true;
}

void YASSApp::OnLaunched() {
  wxApp::OnLaunched();
}

int YASSApp::OnExit() {
  LOG(WARNING) << "Application exiting";
  OnStop();
  return wxApp::OnExit();
}

void YASSApp::OnInitCmdLine(wxCmdLineParser& parser) {
  // initial wxWidgets builtin options through, otherwise it crashes
  parser.SetCmdLine(wxEmptyString);
  wxApp::OnInitCmdLine(parser);
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

void YASSApp::OnStart(bool quiet) {
  state_ = STARTING;
  SaveConfigToDisk();

  std::function<void(asio::error_code)> callback;
  if (!quiet) {
    callback = [](asio::error_code ec) {
      bool successed = false;
      std::string msg;

      if (ec) {
        msg = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      // if the gui library exits, no more need to handle
      if (!wxTheApp) {
        return;
      }

      wxCommandEvent* evt = new wxCommandEvent(
          MY_EVENT, successed ? ID_STARTED : ID_START_FAILED);
      evt->SetString(msg.c_str());
      wxTheApp->QueueEvent(evt);
    };
  }
  worker_.Start(callback);
}

void YASSApp::OnStop(bool quiet) {
  state_ = STOPPING;

  std::function<void()> callback;
  if (!quiet) {
    callback = []() {
      if (wxTheApp) {
        wxCommandEvent* evt = new wxCommandEvent(MY_EVENT, ID_STOPPED);
        wxTheApp->QueueEvent(evt);
      }
    };
  }
  worker_.Stop(callback);
}

void YASSApp::OnStarted(wxCommandEvent& WXUNUSED(event)) {
  state_ = STARTED;
  frame_->Started();
}

void YASSApp::OnStartFailed(wxCommandEvent& event) {
  state_ = START_FAILED;

  error_msg_ = event.GetString();
  LOG(ERROR) << "worker failed due to: " << error_msg_;
  frame_->StartFailed();
}

void YASSApp::OnStopped(wxCommandEvent& WXUNUSED(event)) {
  state_ = STOPPED;
  frame_->Stopped();
}

void YASSApp::LoadConfigFromDisk() {
  config::ReadConfig();
}

void YASSApp::SaveConfigToDisk() {
  auto server_host = frame_->GetServerHost();
  auto server_port = StringToInteger(frame_->GetServerPort());
  auto password = frame_->GetPassword();
  auto method_string = frame_->GetMethod();
  auto method = to_cipher_method(method_string);
  auto local_host = frame_->GetLocalHost();
  auto local_port = StringToInteger(frame_->GetLocalPort());
  auto connect_timeout = StringToInteger(frame_->GetTimeout());

  if (!server_port.ok() || method == CRYPTO_INVALID || !local_port.ok() ||
      !connect_timeout.ok()) {
    LOG(WARNING) << "invalid options";
    return;
  }

  absl::SetFlag(&FLAGS_server_host, server_host);
  absl::SetFlag(&FLAGS_server_port, server_port.value());
  absl::SetFlag(&FLAGS_password, password);
  absl::SetFlag(&FLAGS_cipher_method, method);
  absl::SetFlag(&FLAGS_local_host, local_host);
  absl::SetFlag(&FLAGS_local_port, local_port.value());
  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout.value());

  config::SaveConfig();
}

wxBEGIN_EVENT_TABLE(YASSApp, wxApp)
  EVT_COMMAND(ID_STARTED, MY_EVENT, YASSApp::OnStarted)
  EVT_COMMAND(ID_START_FAILED, MY_EVENT, YASSApp::OnStartFailed)
  EVT_COMMAND(ID_STOPPED, MY_EVENT, YASSApp::OnStopped)
wxEND_EVENT_TABLE()
