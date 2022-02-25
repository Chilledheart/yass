// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#include "win32/yass.hpp"

#include <stdexcept>
#include <string>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <locale.h>

#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crypto/crypter_export.hpp"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass_frame.hpp"
#include "version.h"

ABSL_FLAG(bool, background, false, "start up backgroundd");

#define MULDIVDPI(x) MulDiv(x, uDpi, 96)

// https://docs.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues
BEGIN_MESSAGE_MAP(CYassApp, CWinApp)
  ON_COMMAND(ID_APP_OPTION, &CYassApp::OnAppOption)
  ON_COMMAND(ID_APP_ABOUT, &CYassApp::OnAppAbout)
  ON_THREAD_MESSAGE(WM_MYAPP_STARTED, &CYassApp::OnStarted)
  ON_THREAD_MESSAGE(WM_MYAPP_START_FAILED, &CYassApp::OnStartFailed)
  ON_THREAD_MESSAGE(WM_MYAPP_STOPPED, &CYassApp::OnStopped)
END_MESSAGE_MAP()

CYassApp::CYassApp() = default;
CYassApp::~CYassApp() = default;

CYassApp theApp;

CYassApp* mApp = &theApp;

BOOL CYassApp::InitInstance() {
  if (!SetUTF8Locale()) {
    LOG(WARNING) << "Failed to set up utf-8 locale";
  }

  if (!CheckFirstInstance())
    return FALSE;

  if (!CWinApp::InitInstance())
    return FALSE;

  std::wstring executable_path;
  if (!Utils::GetExecutablePath(&executable_path)) {
    return FALSE;
  }

  absl::InitializeSymbolizer(SysWideToUTF8(executable_path).c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  // TODO move to standalone function
  // Parse command line for internal options
  // https://docs.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinew
  // The lifetime of the returned value is managed by the system, applications should not free or modify this value.
  const wchar_t *cmdline = GetCommandLineW();
  int argc = 0;

  std::unique_ptr<wchar_t*[], decltype(&LocalFree)> wargv(
      CommandLineToArgvW(cmdline, &argc), &LocalFree);
  std::vector<std::string> argv_store(argc);
  std::vector<char*> argv(argc, nullptr);
  for (int i = 0; i != argc; ++i) {
    argv_store[i] = SysWideToUTF8(wargv[i]);
    argv[i] = const_cast<char*>(argv_store[i].data());
  }
  absl::SetFlag(&FLAGS_logtostderr, false);
  absl::ParseCommandLine(argv_store.size(), &argv[0]);

  auto override_cipher_method = to_cipher_method(absl::GetFlag(FLAGS_method));
  if (override_cipher_method != CRYPTO_INVALID) {
    absl::SetFlag(&FLAGS_cipher_method, override_cipher_method);
  }

  LoadConfigFromDisk();
  DCHECK(is_valid_cipher_method(
      static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method))));

  // TODO: transfer OutputDebugString to internal logging

  // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass
  // While the system is starting, the SetThreadPriority function returns a
  // success return value but does not change thread priority for applications
  // that are started from the system Startup folder or listed in the
  // HKLM\...\Run key
  if (!::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)) {
    PLOG(WARNING) << "Failed to set PriorityClass";
  }

  state_ = STOPPED;

  Utils::SetDpiAwareness();

  // Ensure that the common control DLL is loaded.
  InitCommonControls();

  m_pMainWnd = frame_ = new CYassFrame;
  if (frame_ == nullptr) {
    return FALSE;
  }

  SetWindowLongPtrW(frame_->m_hWnd, GWLP_HINSTANCE, (LPARAM)m_hInstance);

  // https://docs.microsoft.com/en-us/windows/win32/menurc/using-menus
  const wchar_t* className = L"yassMainWnd";

  WNDCLASS wndcls;

  // otherwise we need to register a new class
  wndcls.style = CS_DBLCLKS;
  wndcls.lpfnWndProc = DefWindowProc;
  wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
  wndcls.hInstance = m_hInstance;
  wndcls.hIcon = LoadIcon(IDR_MAINFRAME);
  wndcls.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wndcls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wndcls.lpszMenuName = MAKEINTRESOURCE(IDR_MAINFRAME);
  wndcls.lpszClassName = className;

  ::RegisterClassW(&wndcls);

  std::wstring frame_name = LoadStringStdW(m_hInstance, AFX_IDS_APP_TITLE);

  UINT uDpi = Utils::GetDpiForWindowOrSystem(nullptr);
  RECT rect{0, 0, MULDIVDPI(500), MULDIVDPI(400)};

  if (!frame_->Create(className, frame_name.c_str(),
                      WS_MINIMIZEBOX | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                      rect)) {
    LOG(WARNING) << "Failed to create main frame";
    delete frame_;
    return FALSE;
  }

  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
  if (!absl::GetFlag(FLAGS_background)) {
    frame_->ShowWindow(SW_SHOW);
    frame_->SetForegroundWindow();
  } else {
    frame_->ShowWindow(SW_SHOWMINNOACTIVE);
  }
  frame_->UpdateWindow();

  if (Utils::GetAutoStart()) {
    frame_->OnStartButtonClicked();
  }

  if (!MemoryLockAll()) {
    LOG(WARNING) << "Failed to set memory lock";
  }

  LOG(WARNING) << "Application starting: " << YASS_APP_TAG;

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
      std::string* message = nullptr;
      bool ret;

      if (ec) {
        *message = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      // if the gui library exits, no more need to handle
      ret = ::PostThreadMessage(
          main_thread_id, successed ? WM_MYAPP_STARTED : WM_MYAPP_START_FAILED,
          0, reinterpret_cast<LPARAM>(message));
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

void CYassApp::OnAppOption() {
  DialogBoxParamW(m_hInstance, MAKEINTRESOURCE(IDD_OPTIONBOX),
                  frame_->m_hWnd, &CYassApp::OnAppOptionMessage,
                  reinterpret_cast<LPARAM>(this));
}

// static
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-dlgproc
INT_PTR CALLBACK CYassApp::OnAppOptionMessage(HWND hDlg, UINT message,
                                              WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  auto self =
    reinterpret_cast<CYassApp*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));

  switch (message) {
    case WM_INITDIALOG: {
      SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
      // extra initialization to all fields
      auto connect_timeout = absl::GetFlag(FLAGS_connect_timeout);
      auto tcp_user_timeout = absl::GetFlag(FLAGS_tcp_user_timeout);
      auto tcp_so_linger_timeout = absl::GetFlag(FLAGS_so_linger_timeout);
      auto tcp_so_snd_buffer = absl::GetFlag(FLAGS_so_snd_buffer);
      auto tcp_so_rcv_buffer = absl::GetFlag(FLAGS_so_rcv_buffer);
      SetDlgItemInt(hDlg, IDC_EDIT_CONNECT_TIMEOUT, connect_timeout, FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_USER_TIMEOUT, tcp_user_timeout, FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_LINGER_TIMEOUT, tcp_so_linger_timeout, FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_SEND_BUFFER, tcp_so_snd_buffer, FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_RECEIVE_BUFFER, tcp_so_rcv_buffer, FALSE);
      return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK) {
        BOOL translated;
        // TODO prompt a fix-me tip
        auto connect_timeout = GetDlgItemInt(hDlg, IDC_EDIT_CONNECT_TIMEOUT, &translated, FALSE);
        if (translated == FALSE)
          return (INT_PTR)FALSE;
        auto tcp_user_timeout = GetDlgItemInt(hDlg, IDC_EDIT_TCP_USER_TIMEOUT, &translated, FALSE);
        if (translated == FALSE)
          return (INT_PTR)FALSE;
        auto tcp_so_linger_timeout = GetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_LINGER_TIMEOUT, &translated, FALSE);
        if (translated == FALSE)
          return (INT_PTR)FALSE;
        auto tcp_so_snd_buffer = GetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_SEND_BUFFER, &translated, FALSE);
        if (translated == FALSE)
          return (INT_PTR)FALSE;
        auto tcp_so_rcv_buffer = GetDlgItemInt(hDlg, IDC_EDIT_TCP_SO_RECEIVE_BUFFER, &translated, FALSE);
        if (translated == FALSE)
          return (INT_PTR)FALSE;
        absl::SetFlag(&FLAGS_connect_timeout, connect_timeout);
        absl::SetFlag(&FLAGS_tcp_user_timeout, tcp_user_timeout);
        absl::SetFlag(&FLAGS_so_linger_timeout, tcp_so_linger_timeout);
        absl::SetFlag(&FLAGS_so_snd_buffer, tcp_so_snd_buffer);
        absl::SetFlag(&FLAGS_so_rcv_buffer, tcp_so_rcv_buffer);
        self->SaveConfigToDisk();
      }
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
    default:
      break;
  }
  return (INT_PTR)FALSE;
}

void CYassApp::OnAppAbout() {
  DialogBoxParamW(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX),
                  frame_->m_hWnd, &CYassApp::OnAppAboutMessage, 0L);
}

// static
INT_PTR CALLBACK CYassApp::OnAppAboutMessage(HWND hDlg, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK) {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
    default:
      break;
  }
  return (INT_PTR)FALSE;
}

// https://docs.microsoft.com/en-us/windows/win32/winprog/windows-data-types?redirectedfrom=MSDN
void CYassApp::OnStarted(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STARTED;
  frame_->OnStarted();
}

void CYassApp::OnStartFailed(WPARAM w, LPARAM l) {
  state_ = START_FAILED;
  std::unique_ptr<std::string> message_ptr(reinterpret_cast<std::string*>(l));

  error_msg_ = std::move(message_ptr.operator*());

  LOG(ERROR) << "worker failed due to: " << error_msg_;
  frame_->OnStartFailed();
}

void CYassApp::OnStopped(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STOPPED;
  frame_->OnStopped();
}

BOOL CYassApp::CheckFirstInstance() {
  std::wstring app_name = LoadStringStdW(m_hInstance, AFX_IDS_APP_TITLE);

  HWND first_wnd = FindWindow(nullptr, app_name.c_str());

  // another instance is already running - activate it
  if (first_wnd) {
    HWND popup_wnd = GetLastActivePopup(first_wnd);
    SetForegroundWindow(popup_wnd);
    if (IsIconic(popup_wnd))
      ShowWindow(popup_wnd, SW_SHOWNORMAL);
    if (first_wnd != popup_wnd)
      SetForegroundWindow(popup_wnd);
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
