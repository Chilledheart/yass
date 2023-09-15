// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2022 Chilledheart  */
#include "gtk/yass.hpp"

#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_cat.h>
#include <fontconfig/fontconfig.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <stdarg.h>

#include "core/io_queue.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "core/cxx17_backports.hpp"
#include "crypto/crypter_export.hpp"
#include "gtk/utils.hpp"
#include "gtk/yass_window.hpp"
#include "version.h"

ABSL_FLAG(bool, background, false, "start up backgroundd");

YASSApp* mApp = nullptr;

static const char* kAppId = "it.gui.yass";
static const char* kAppName = YASS_APP_PRODUCT_NAME;

static std::string _GetExecutablePath(const std::string& fallback) {
  char buf[PATH_MAX+1];
  ssize_t ret = readlink("/proc/self/exe", buf, sizeof(buf)-1);
  if (ret < 0) {
    return fallback;
  }
  return std::string(buf, ret);
}

int main(int argc, const char** argv) {
  SetExecutablePath(_GetExecutablePath(argv[0]));
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }
  std::string locale_path = "../share/locale";
  size_t rpos = exec_path.rfind('/');
  if (rpos != std::string::npos)
    locale_path = exec_path.substr(0, rpos + 1) + locale_path;
  setlocale(LC_ALL, "");
  bindtextdomain("yass", locale_path.c_str());
  textdomain("yass");

  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetProgramUsageMessage(
      absl::StrCat("Usage: ", Basename(exec_path), " [options ...]\n",
                   " -c, --configfile <file> Use specified config file\n",
                   " --server_host <host> Host address which remote server listens to\n",
                   " --server_port <port> Port number which remote server listens to\n",
                   " --local_host <host> Host address which local server listens to\n"
                   " --local_port <port> Port number which local server listens to\n"
                   " --username <username> Username\n",
                   " --password <pasword> Password pharsal\n",
                   " --method <method> Method of encrypt"));
  config::ReadConfigFileOption(argc, argv);
  config::ReadConfig();
  absl::ParseCommandLine(argc, const_cast<char**>(argv));
  IoQueue::set_allow_merge(absl::GetFlag(FLAGS_io_queue_allow_merge));

#if !GLIB_CHECK_VERSION(2, 35, 0)
  // GLib type system initialization. It's unclear if it's still required for
  // any remaining code. Most likely this is superfluous as gtk_init() ought
  // to do this. It's definitely harmless, so it's retained here.
  g_type_init();
#endif  // !GLIB_CHECK_VERSION(2, 35, 0)

  SetUpGLibLogHandler();

  auto app = YASSApp::create();

  mApp = app.operator->();

  return app->ApplicationRun(1, const_cast<char**>(argv));
}

YASSApp::YASSApp()
#if GLIB_CHECK_VERSION(2, 74, 0)
    : impl_(gtk_application_new(kAppId, G_APPLICATION_DEFAULT_FLAGS)),
#else
    : impl_(gtk_application_new(kAppId, G_APPLICATION_FLAGS_NONE)),
#endif
      idle_source_(g_timeout_source_new(200)) {
  g_set_application_name(kAppName);

  gdk_init(nullptr, nullptr);
  gtk_init(nullptr, nullptr);

  auto activate = []() { mApp->OnActivate(); };
  g_signal_connect(impl_, "activate", G_CALLBACK(activate), NULL);

  auto idle = [](gpointer user_data) -> gboolean {
    if (!mApp) {
      return G_SOURCE_REMOVE;
    }
    mApp->OnIdle();
    return G_SOURCE_CONTINUE;
  };
  g_source_set_priority(idle_source_, G_PRIORITY_LOW);
  g_source_set_callback(idle_source_, idle, this, nullptr);
  g_source_set_name(idle_source_, "Idle Source");
  g_source_attach(idle_source_, nullptr);
  g_source_unref(idle_source_);
}

YASSApp::~YASSApp() = default;

std::unique_ptr<YASSApp> YASSApp::create() {
  return std::unique_ptr<YASSApp>(new YASSApp);
}

void YASSApp::OnActivate() {
  if (!MemoryLockAll()) {
    LOG(WARNING) << "Failed to set memory lock";
  }

  if (!dispatcher_.Init([this]() { OnDispatch(); })) {
    LOG(WARNING) << "Failed to init dispatcher";
  }

  main_window_ = new YASSWindow();
  main_window_->show();
  // https://docs.gtk.org/gtk3/method.Window.present.html
  if (!absl::GetFlag(FLAGS_background)) {
    main_window_->present();
  }
  gtk_application_add_window(impl_, main_window_->impl_);

  if (Utils::GetAutoStart()) {
    main_window_->OnStartButtonClicked();
  }
}

int YASSApp::ApplicationRun(int argc, char** argv) {
  int ret = g_application_run(G_APPLICATION(impl_), argc, argv);

  if (ret) {
    LOG(WARNING) << "app exited with code " << ret;
  }

  LOG(WARNING) << "Application exiting";

  // Memory leak clean up path
  pango_cairo_font_map_set_default(nullptr);
  cairo_debug_reset_static_data();
  FcFini();

  return ret;
}

void YASSApp::Exit() {
  if (!mApp) {
    return;
  }
  mApp = nullptr;
  g_source_destroy(idle_source_);
}

void YASSApp::OnIdle() {
  main_window_->UpdateStatusBar();
}

std::string YASSApp::GetStatus() const {
  std::ostringstream ss;
  if (state_ == STARTED) {
    ss << _("Connected with conns: ") << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    ss << _("Failed to connect due to ") << error_msg_.c_str();
  } else {
    ss << _("Disconnected with ") << worker_.GetRemoteDomain();
  }
  return ss.str();
}

void YASSApp::OnStart(bool quiet) {
  state_ = STARTING;
  SaveConfig();

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
        dispatch_queue_.emplace(successed ? STARTED : START_FAILED, msg);
      }

      dispatcher_.Emit();
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
        dispatch_queue_.emplace(STOPPED, std::string());
      }

      dispatcher_.Emit();
    };
  }
  worker_.Stop(callback);
}

void YASSApp::OnStarted() {
  state_ = STARTED;
  config::SaveConfig();
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

void YASSApp::SaveConfig() {
  auto server_host = main_window_->GetServerHost();
  auto server_port = StringToInteger(main_window_->GetServerPort());
  auto username = main_window_->GetUsername();
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
  absl::SetFlag(&FLAGS_username, username);
  absl::SetFlag(&FLAGS_password, password);
  absl::SetFlag(&FLAGS_method, method);
  absl::SetFlag(&FLAGS_local_host, local_host);
  absl::SetFlag(&FLAGS_local_port, local_port.value());
  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout.value());
}
