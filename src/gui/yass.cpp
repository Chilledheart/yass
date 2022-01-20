// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#include "gui/yass.hpp"

#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "gui/yass_window.hpp"

YASSApp* mApp = nullptr;

static const char* kAppId = "it.gui.yass";
static const char* kAppName = "Yet Another Shadow Socket";

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::ParseCommandLine(argc, argv);

  auto cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, cipher_method);
  }

  config::ReadConfig();

  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  auto app = YASSApp::create();

  mApp = app.get();

  return app->ApplicationRun();
}

YASSApp::YASSApp()
    : Gtk::Application(kAppId),
      dispatcher_(Glib::MainContext::get_thread_default()) {
  ::g_set_application_name(kAppName);
}

YASSApp::~YASSApp() {
  delete main_window_;
}

Glib::RefPtr<YASSApp> YASSApp::create() {
  return Glib::RefPtr<YASSApp>(new YASSApp());
}

void YASSApp::on_startup() {
  LOG(WARNING) << "Application starting";

  Gtk::Application::on_startup();

  idle_connection_ = Glib::signal_idle().connect(
      sigc::mem_fun(*this, &YASSApp::OnIdle), Glib::PRIORITY_LOW);

  dispatcher_.connect(sigc::mem_fun(*this, &YASSApp::OnDispatch));
}

void YASSApp::on_activate() {
  Gtk::Application::on_activate();

  // main window is created with WINDOW_TOPLEVEL by defaults
  main_window_ = new YASSWindow();
  main_window_->show();
  main_window_->activate_focus();
  main_window_->present();
}

int YASSApp::ApplicationRun() {
  int ret = run(*main_window_);
  if (ret) {
    LOG(WARNING) << "app exited with code " << ret;
  }
  LOG(WARNING) << "Application exiting";
  return ret;
}

void YASSApp::Exit() {
  idle_connection_.disconnect();
  quit();
}

bool YASSApp::OnIdle() {
  if (GetState() == YASSApp::STARTED) {
    main_window_->UpdateStatusBar();
  }
  return true;
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
    callback = [this](asio::error_code ec) {
      bool successed = false;
      std::string msg;

      if (ec) {
        msg = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      {
        absl::MutexLock lk(&dispatch_mutex_);
        dispatch_queue_.push(
            std::make_pair(successed ? STARTED : START_FAILED, msg));
      }

      dispatcher_.emit();
    };
  }
  worker_.Start(callback);
}

void YASSApp::OnStop(bool quiet) {
  state_ = STOPPING;

  std::function<void()> callback;
  if (!quiet) {
    callback = [this]() {
      {
        absl::MutexLock lk(&dispatch_mutex_);
        dispatch_queue_.push(std::make_pair(STOPPED, std::string()));
      }

      dispatcher_.emit();
    };
  }
  worker_.Stop(callback);
}

void YASSApp::OnStarted() {
  state_ = STARTED;
  main_window_->Started();
}

void YASSApp::OnStartFailed(const std::string& error_msg) {
  state_ = START_FAILED;

  error_msg_ = error_msg;
  LOG(ERROR) << "worker failed due to: " << error_msg_;
  main_window_->StartFailed();
}

void YASSApp::OnStopped() {
  state_ = STOPPED;
  main_window_->Stopped();
}

void YASSApp::OnDispatch() {
  std::pair<YASSState, std::string> event;
  {
    absl::MutexLock lk(&dispatch_mutex_);
    event = dispatch_queue_.front();
    dispatch_queue_.pop();
  }
  if (event.first == STARTED)
    OnStarted();
  else if (event.first == START_FAILED)
    OnStartFailed(event.second);
  else if (event.first == STOPPED)
    OnStopped();
}

void YASSApp::SaveConfigToDisk() {
  auto server_host = main_window_->GetServerHost();
  auto server_port = StringToInteger(main_window_->GetServerPort());
  auto password = main_window_->GetPassword();
  auto method_string = main_window_->GetMethod();
  auto method = to_cipher_method(method_string);
  auto local_host = main_window_->GetLocalHost();
  auto local_port = StringToInteger(main_window_->GetLocalPort());
  auto connect_timeout = StringToInteger(main_window_->GetTimeout());

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
