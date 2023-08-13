// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */
#include "gtk4/yass.hpp"

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
#include "gtk4/yass_window.hpp"
#include "version.h"
#include "gtk4/option_dialog.hpp"

ABSL_FLAG(bool, background, false, "start up backgroundd");

YASSApp* mApp = nullptr;

static const char* kAppId = "it.gui.yass";
static const char* kAppName = YASS_APP_PRODUCT_NAME;

extern "C" {

struct _YASSGtkApp
{
  GtkApplication parent;
  YASSApp *thiz;
};

G_DEFINE_TYPE(YASSGtkApp, yass_app, GTK_TYPE_APPLICATION)

static void
yass_app_init(YASSGtkApp *app)
{
}

static void
option_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       app)
{
  YASSGtk_APP(app)->thiz->OnOption();
}

static void
about_activated (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       app)
{
  YASSGtk_APP(app)->thiz->OnAbout();
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
  YASSGtk_APP(app)->thiz->Exit();
}

static GActionEntry app_entries[] =
{
  { "option", option_activated, NULL, NULL, NULL },
  { "about", about_activated, NULL, NULL, NULL },
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
yass_app_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };

  G_APPLICATION_CLASS(yass_app_parent_class)->startup(app);

  g_action_map_add_action_entries(G_ACTION_MAP(app),
                                  app_entries, G_N_ELEMENTS (app_entries),
                                  app);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app),
                                        "app.quit",
                                        quit_accels);
}

static void
yass_app_activate(GApplication *app)
{
  YASSGtk_APP(app)->thiz->OnActivate();
}

static void
yass_app_class_init(YASSGtkAppClass *cls)
{
  G_APPLICATION_CLASS(cls)->startup = yass_app_startup;
  G_APPLICATION_CLASS(cls)->activate = yass_app_activate;
}

YASSGtkApp*
yass_app_new (void)
{
  return YASSGtk_APP(g_object_new (yass_app_get_type(),
                                   "application-id", kAppId, NULL));
}
} // extern "C"

int main(int argc, const char** argv) {
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }
  setlocale(LC_ALL, "");
  bindtextdomain("yass", "../share/locale");
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

  mApp = app.get();

  return app->ApplicationRun(1, const_cast<char**>(argv));
}

YASSApp::YASSApp()
    : impl_(G_APPLICATION(yass_app_new())),
      idle_source_(g_timeout_source_new(200)) {
  YASSGtk_APP(impl_)->thiz = this;
  g_set_application_name(kAppName);

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

  main_window_ = new YASSWindow(impl_);
  main_window_->show();

  if (Utils::GetAutoStart()) {
    main_window_->OnStartButtonClicked();
  }

  // https://docs.gtk.org/gtk3/method.Window.present.html
  if (!absl::GetFlag(FLAGS_background)) {
    main_window_->present();
  }
}

int YASSApp::ApplicationRun(int argc, char** argv) {
  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;

  int ret = g_application_run(G_APPLICATION(impl_), argc, argv);

  if (ret) {
    LOG(WARNING) << "app exited with code " << ret;
  } else {
    LOG(WARNING) << "Application exiting";
  }

#if 0
  // Memory leak clean up path
  pango_cairo_font_map_set_default(nullptr);
  cairo_debug_reset_static_data();
  FcFini();
#endif

  return ret;
}

void YASSApp::Exit() {
  if (!mApp) {
    return;
  }
  mApp = nullptr;
  g_source_destroy(idle_source_);
  g_application_quit(G_APPLICATION(impl_));
}

void YASSApp::OnIdle() {
  main_window_->UpdateStatusBar();
}

std::string YASSApp::GetStatus() const {
  std::ostringstream ss;
  if (state_ == STARTED) {
    ss << "Connected with conns: " << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    ss << "Failed to connect due to " << error_msg_.c_str();
  } else {
    ss << "Disconnected with " << worker_.GetRemoteDomain();
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

void YASSApp::OnAbout() {
  GtkAboutDialog* about_dialog = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
  const char* artists[] = {"macosicons.com", nullptr};
  gtk_about_dialog_set_artists(about_dialog, artists);
  const char* authors[] = {YASS_APP_COMPANY_NAME, nullptr};
  gtk_about_dialog_set_authors(about_dialog, authors);
  gtk_about_dialog_set_comments(about_dialog, "Last Change: " YASS_APP_LAST_CHANGE);
  gtk_about_dialog_set_copyright(about_dialog, YASS_APP_COPYRIGHT);
  gtk_about_dialog_set_license_type(about_dialog, GTK_LICENSE_GPL_2_0);
  gtk_about_dialog_set_logo_icon_name(about_dialog, "yass");
  gtk_about_dialog_set_program_name(about_dialog, YASS_APP_PRODUCT_NAME);
  gtk_about_dialog_set_version(about_dialog, YASS_APP_PRODUCT_VERSION);
  gtk_about_dialog_set_website(about_dialog, YASS_APP_WEBSITE);
  gtk_about_dialog_set_website_label(about_dialog, "official-site");

  gtk_window_present(GTK_WINDOW(about_dialog));
}

void YASSApp::OnOption() {
  auto dialog = new OptionDialog("YASS Option",
                                 GTK_WINDOW(main_window_->impl()), true);

  dialog->run();
}
