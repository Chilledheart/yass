// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#include "win32/yass.hpp"

#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass_frame.hpp"

// https://docs.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues
// https://docs.microsoft.com/en-us/cpp/mfc/reference/message-map-macros-mfc?view=msvc-170#on_thread_message
BEGIN_MESSAGE_MAP(CYassApp, CWinApp)
  ON_THREAD_MESSAGE(WM_MYAPP_STARTED, &CYassApp::OnStarted)
  ON_THREAD_MESSAGE(WM_MYAPP_START_FAILED, &CYassApp::OnStartFailed)
  ON_THREAD_MESSAGE(WM_MYAPP_STOPPED, &CYassApp::OnStopped)
END_MESSAGE_MAP()

CYassApp::CYassApp() = default;
CYassApp::~CYassApp() = default;

CYassApp theApp;

CYassApp* mApp = &theApp;

BOOL CYassApp::InitInstance() {
  if (!CheckFirstInstance())
    return FALSE;

  if (!CWinApp::InitInstance())
    return FALSE;

  // TODO move to utils
  std::wstring appPath(MAX_PATH + 1, L'\0');
  ::GetModuleFileNameW(nullptr, &appPath[0], MAX_PATH);
  absl::InitializeSymbolizer(SysWideToUTF8(appPath).c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  // TODO move to standalone function
  // Parse command line for internal options
  LPWSTR* warglist;
  int num_arg = 0;

  warglist = ::CommandLineToArgvW(::GetCommandLineW(), &num_arg);
  auto arglist = std::make_unique<char*[]>(num_arg);
  std::vector<std::string> argv;
  for (int current_arg = 0; current_arg != num_arg; ++current_arg) {
    argv.push_back(SysWideToUTF8(warglist[current_arg]));
    arglist[current_arg] = const_cast<char*>(argv[current_arg].data());
  }
  absl::ParseCommandLine(argv.size(), arglist.get());

  auto override_cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (override_cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, override_cipher_method);
  }

  LoadConfigFromDisk();
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  // TODO: transfer OutputDebugString to internal logging
#if 0
  wxLog* logger = new YASSLog;
  wxLog::SetActiveTarget(logger);
#endif

  LOG(WARNING) << "Application starting";

  state_ = STOPPED;

  if (Utils::SetProcessDpiAwareness()) {
    LOG(WARNING) << "SetProcessDpiAwareness applied";
  }

  m_pMainWnd = frame_ = new CYassFrame;
  if (frame_ == nullptr) {
    return FALSE;
  }

  HICON icon = LoadIcon(IDR_MAINFRAME);

  LPCTSTR lpszClass =
      AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                          (HBRUSH)(COLOR_BTNFACE + 1), icon);
  CString frame_name;
  if (!frame_name.LoadString(AFX_IDS_APP_TITLE)) {
    LOG(WARNING) << "frame name not loaded";
    delete frame_;
    return FALSE;
  }

  RECT rect{0, 0, 400 * 2, 300 * 2};

  if (!frame_->Create(lpszClass, frame_name, WS_OVERLAPPED | WS_CAPTION |
                      WS_SYSMENU, rect)) {
    LOG(WARNING) << "Failed to create main frame";
    delete frame_;
    return FALSE;
  }

  frame_->CenterWindow();
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
  frame_->ShowWindow(SW_SHOW);
  frame_->UpdateStatus();
  frame_->UpdateWindow();

  if (Utils::GetAutoStart()) {
    frame_->OnStartButtonClicked();
  }

  return TRUE;
}

int CYassApp::ExitInstance() {
  LOG(WARNING) << "Application exiting";
  worker_.Stop([]() {});
  return CWinApp::ExitInstance();
}

std::string CYassApp::GetStatus() const {
  std::stringstream ss;
  if (state_ == STARTED) {
    ss << "Connected with conns: " << worker_.currentConnections();
  } else if (state_ == START_FAILED) {
    ss << "Failed to connect due to " << error_msg_;
  } else {
    ss << "Disconnected with " << worker_.GetRemoteEndpoint();
  }
  return ss.str();
};

void CYassApp::OnStart(bool quiet) {
  DWORD main_thread_id = GetCurrentThreadId();
  state_ = STARTING;
  SaveConfigToDisk();

  std::function<void(asio::error_code)> callback;
  if (!quiet) {
    callback = [main_thread_id](asio::error_code ec) {
      bool successed = false;
      char* message = nullptr;
      int message_size = 0;
      bool ret;

      if (ec) {
        std::string message_storage = ec.message();
        message_size = message_storage.size();
        message = new char[message_size + 1];
        memcpy(message, message_storage.c_str(), message_size + 1);
        successed = false;
      } else {
        successed = true;
      }

      // if the gui library exits, no more need to handle
      ret = ::PostThreadMessage(
          main_thread_id, successed ? WM_MYAPP_STARTED : WM_MYAPP_START_FAILED,
          static_cast<WPARAM>(message_size), reinterpret_cast<LPARAM>(message));
      if (!ret) {
        LOG(WARNING) << "Failed to post message to main thread: " << std::hex
                     << ::GetLastError() << std::dec;
      }
    };
  }
  worker_.Start(callback);
}

void CYassApp::OnStop(bool quiet) {
  DWORD main_thread_id = GetCurrentThreadId();
  state_ = STOPPING;
  std::function<void()> callback;
  if (!quiet) {
    callback = [main_thread_id]() {
      bool ret;
      ret = ::PostThreadMessage(main_thread_id, WM_MYAPP_STOPPED, 0, 0);
      if (!ret) {
        LOG(WARNING) << "Failed to post message to main thread: " << std::hex
                     << ::GetLastError() << std::dec;
      }
    };
  }
  worker_.Stop(callback);
}

// https://docs.microsoft.com/en-us/windows/win32/winprog/windows-data-types?redirectedfrom=MSDN
void CYassApp::OnStarted(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STARTED;
  frame_->OnStarted();
}

void CYassApp::OnStartFailed(WPARAM w, LPARAM l) {
  state_ = START_FAILED;
  char* message = reinterpret_cast<char*>(l);
  int message_size = static_cast<int>(w);

  error_msg_ = std::string(message, message_size);
  delete[] message;
  LOG(ERROR) << "worker failed due to: " << error_msg_;
  frame_->OnStartFailed();
}

void CYassApp::OnStopped(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STOPPED;
  frame_->OnStopped();
}

BOOL CYassApp::CheckFirstInstance() {
  CString app_name;
  if (!app_name.LoadString(AFX_IDS_APP_TITLE)) {
    LOG(WARNING) << "app name not loaded";
    return FALSE;
  }

  CWnd* first_wnd = CWnd::FindWindow(nullptr, app_name);

  // another instance is already running - activate it
  if (first_wnd) {
    CWnd* popup_wnd = first_wnd->GetLastActivePopup();
    popup_wnd->SetForegroundWindow();
    if (popup_wnd->IsIconic())
      popup_wnd->ShowWindow(SW_SHOWNORMAL);
    if (first_wnd != popup_wnd)
      popup_wnd->SetForegroundWindow();
    return FALSE;
  }

  // this is the first instance
  return TRUE;
}

void CYassApp::LoadConfigFromDisk() {
  config::ReadConfig();
}

void CYassApp::SaveConfigToDisk() {
  auto server_host = frame_->GetServerHost();
  auto server_port = StringToInteger(frame_->GetServerPort());
  auto password = frame_->GetPassword();
  auto method = frame_->GetMethod();
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
