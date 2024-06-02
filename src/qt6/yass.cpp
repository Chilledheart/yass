// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "qt6/yass.hpp"
#include "qt6/tray_icon.hpp"
#include "qt6/yass_window.hpp"

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <locale.h>
#include <stdarg.h>
#include <QLibraryInfo>
#include <QTimer>
#include <QTranslator>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
#include "feature.h"
#include "freedesktop/utils.hpp"
#include "version.h"

int main(int argc, const char** argv) {
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  // Set C library locale to make sure CommandLine can parse
  // argument values in the correct encoding and to make sure
  // generated file names (think downloads) are in the file system's
  // encoding.
  setlocale(LC_ALL, "");
  // For numbers we never want the C library's locale sensitive
  // conversion from number to string because the only thing it
  // changes is the decimal separator which is not good enough for
  // the UI and can be harmful elsewhere.
  setlocale(LC_NUMERIC, "C");

  absl::InitializeSymbolizer(exec_path.c_str());
#ifdef HAVE_CRASHPAD
  CHECK(InitializeCrashpad(exec_path));
#else
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);
#endif

  config::SetClientUsageMessage(exec_path);
  config::ReadConfigFileAndArguments(argc, argv);

  CRYPTO_library_init();

  YASSApp program(argc, const_cast<char**>(argv));

  if (!program.Init()) {
    return 0;
  }

  return program.exec();
}

YASSApp::YASSApp(int& argc, char** argv) : QApplication(argc, argv) {
  QObject::connect(this, &YASSApp::OnStartedSignal, this, &YASSApp::OnStarted);
  QObject::connect(this, &YASSApp::OnStartFailedSignal, this, &YASSApp::OnStartFailed);
  QObject::connect(this, &YASSApp::OnStoppedSignal, this, &YASSApp::OnStopped);
}

bool YASSApp::Init() {
  qt_translator_ = new QTranslator(this);
  my_translator_ = new QTranslator(this);
  // TODO changable locale
  QLocale locale = QLocale::system();

#if defined(_WIN32)
  (void)qt_translator_->load("qt_" + locale.name());
#else
  (void)qt_translator_->load("qt_" + locale.name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#endif

  if (!my_translator_->load(QString(":/lang/yass_%1.qm").arg(locale.name()))) {
    LOG(ERROR) << "Failed to find language resource: " << locale.name().toUtf8().data()
               << " fallback to en_us language";
    (void)my_translator_->load(":/lang/yass_en.qm");
  }
  qApp->installTranslator(qt_translator_);
  qApp->installTranslator(my_translator_);

  main_window_ = new YASSWindow();
  main_window_->show();

  tray_icon_ = new TrayIcon(this);
  tray_icon_->show();

  worker_ = std::make_unique<Worker>();

  if (Utils::GetAutoStart()) {
    main_window_->OnStartButtonClicked();
  }
  idle_timer_ = new QTimer(this);
  idle_timer_->setInterval(100);
  QObject::connect(idle_timer_, &QTimer::timeout, this, &YASSApp::OnIdle);

  idle_timer_->start();

  return true;
}

void YASSApp::OnIdle() {
  main_window_->UpdateStatusBar();
}

std::string YASSApp::GetStatus() const {
  std::ostringstream ss;
  if (state_ == STARTED) {
    ss << tr("Connected with conns: ").toUtf8().data() << worker_->currentConnections();
  } else if (state_ == STARTING) {
    ss << tr("Connecting").toUtf8().data();
  } else if (state_ == START_FAILED) {
    ss << tr("Failed to connect due to ").toUtf8().data() << error_msg_.c_str();
  } else if (state_ == STOPPING) {
    ss << tr("Disconnecting").toUtf8().data();
  } else {
    ss << tr("Disconnected with ").toUtf8().data() << worker_->GetRemoteDomain();
  }
  return ss.str();
}

void YASSApp::OnStart(bool quiet) {
  state_ = STARTING;
  std::string err_msg = SaveConfig();
  if (!err_msg.empty()) {
    OnStartFailed(err_msg);
    return;
  }

  absl::AnyInvocable<void(asio::error_code)> callback;
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

      if (successed)
        emit OnStartedSignal();
      else
        emit OnStartFailedSignal(msg);
    };
  }
  worker_->Start(std::move(callback));
}

void YASSApp::OnStop(bool quiet) {
  state_ = STOPPING;

  absl::AnyInvocable<void()> callback;
  if (!quiet) {
    callback = [this]() { emit OnStoppedSignal(); };
  }
  worker_->Stop(std::move(callback));
}

void YASSApp::OnStarted() {
  state_ = STARTED;
  config::SaveConfig();
  main_window_->Started();
}

void YASSApp::OnStartFailed(const std::string& error_msg) {
  state_ = START_FAILED;

  error_msg_ = error_msg;
  main_window_->StartFailed();
}

void YASSApp::OnStopped() {
  state_ = STOPPED;
  main_window_->Stopped();
}

std::string YASSApp::SaveConfig() {
  auto server_host = main_window_->GetServerHost();
  auto server_sni = main_window_->GetServerSNI();
  auto server_port = main_window_->GetServerPort();
  auto username = main_window_->GetUsername();
  auto password = main_window_->GetPassword();
  auto method_string = main_window_->GetMethod();
  auto local_host = main_window_->GetLocalHost();
  auto local_port = main_window_->GetLocalPort();
  auto doh_url = main_window_->GetDoHUrl();
  auto dot_host = main_window_->GetDoTHost();
  auto connect_timeout = main_window_->GetTimeout();

  return config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method_string,
                                        local_host, local_port, doh_url, dot_host, connect_timeout);
}
