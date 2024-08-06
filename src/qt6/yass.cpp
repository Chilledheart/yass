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
#include <signal.h>
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

namespace config {
const ProgramType pType = YASS_CLIENT_GUI;
}  // namespace config

ABSL_FLAG(bool, background, false, "start up background");

#ifdef _WIN32
void YASSApp::commitData(QSessionManager& manager) {
  if (auto main = App()->main_window_.get()) {
    QMetaObject::invokeMethod(main, "close", Qt::QueuedConnection);
    manager.cancel();
  }
}
#endif

int main(int argc, const char** argv) {
#ifndef _WIN32
  // setup signal handler
  signal(SIGPIPE, SIG_IGN);

  struct sigaction sig_handler;

  sig_handler.sa_handler = YASSApp::SigIntSignalHandler;
  sigemptyset(&sig_handler.sa_mask);
  sig_handler.sa_flags = 0;

  sigaction(SIGINT, &sig_handler, nullptr);

  /* Block SIGPIPE in all threads, this can happen if a thread calls write on
     a closed pipe. */
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask failed");
    return -1;
  }
#endif
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

  // call program init
  if (!program.Init()) {
    return 0;
  }

  // enter event loop
  return program.exec();
}

#ifndef _WIN32
int YASSApp::sigintFd[2] = {-1, -1};
#endif

#ifndef _WIN32
void YASSApp::SigIntSignalHandler(int /*s*/) {
  /* Handles SIGINT and writes to a socket. Qt will read
   * from the socket in the main thread event loop and trigger
   * a call to the ProcessSigInt slot, where we can safely run
   * shutdown code without signal safety issues. */

  char a = 1;
  send(sigintFd[1], &a, sizeof(a), 0);
}
#endif

#ifndef _WIN32
void YASSApp::ProcessSigInt() {
  char tmp;
  recv(sigintFd[0], &tmp, sizeof(tmp), 0);

  App()->quit();
}
#endif

YASSApp::YASSApp(int& argc, char** argv) : QApplication(argc, argv) {
#ifndef _WIN32
  /* Handle SIGINT properly */
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd) < 0) {
    PLOG(FATAL) << "socketpair failure at startup";
  }
  snInt = new QSocketNotifier(sigintFd[0], QSocketNotifier::Read, this);
  connect(snInt, &QSocketNotifier::activated, this, &YASSApp::ProcessSigInt);
#else
  connect(qApp, &QGuiApplication::commitDataRequest, this, &YASSApp::commitData);
#endif

  setApplicationVersion(YASS_APP_TAG);
#ifndef __APPLE__
  setWindowIcon(QIcon::fromTheme("it.gui.yass", QIcon(":/res/images/it.gui.yass.png")));
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
  setDesktopFileName("it.gui.yass");
#endif
}

bool YASSApp::Init() {
  QObject::connect(this, &QCoreApplication::aboutToQuit, this, &YASSApp::OnQuit);

  qt_translator_ = new QTranslator(this);
  my_translator_ = new QTranslator(this);
  // TODO changable locale
  QLocale locale = QLocale::system();

#if defined(_WIN32)
  (void)qt_translator_->load("qt_" + locale.name());
#else
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  (void)qt_translator_->load("qt_" + locale.name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#else
  (void)qt_translator_->load("qt_" + locale.name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
#endif

  if (!my_translator_->load(QString(":/lang/yass_%1.qm").arg(locale.name()))) {
    LOG(ERROR) << "Failed to find language resource: " << locale.name().toUtf8().data()
               << " fallback to en_us language";
    (void)my_translator_->load(":/lang/yass_en.qm");
  }
  qApp->installTranslator(qt_translator_);
  qApp->installTranslator(my_translator_);

  QObject::connect(this, &YASSApp::OnStartedSignal, this, &YASSApp::OnStarted);
  QObject::connect(this, &YASSApp::OnStartFailedSignal, this, &YASSApp::OnStartFailed);
  QObject::connect(this, &YASSApp::OnStoppedSignal, this, &YASSApp::OnStopped);

  main_window_ = std::make_unique<YASSWindow>();
  main_window_->show();
  main_window_->moveToCenter();

  if (absl::GetFlag(FLAGS_background)) {
    main_window_->hide();
  }

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

YASSApp::~YASSApp() {
  delete snInt;
  close(sigintFd[0]);
  close(sigintFd[1]);
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

void YASSApp::OnQuit() {
  LOG(WARNING) << "Application Exit";
  idle_timer_->stop();
  PrintMallocStats();
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
  auto limit_rate = main_window_->GetLimitRate();
  auto connect_timeout = main_window_->GetTimeout();

  return config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method_string,
                                        local_host, local_port, doh_url, dot_host, limit_rate, connect_timeout);
}
