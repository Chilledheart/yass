// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */
#include "win32/yass.hpp"

#include <stdexcept>
#include <string>

#include <shellapi.h>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/strings/str_cat.h>
#include <locale.h>
#include <iostream>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#include "core/debug.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "crashpad_helper.hpp"
#include "crypto/crypter_export.hpp"
#include "version.h"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass_frame.hpp"

ABSL_FLAG(bool, background, false, "start up backgroundd");

#define MULDIVDPI(x) MulDiv(x, uDpi, 96)

CYassApp::CYassApp(HINSTANCE hInstance) : m_hInstance(hInstance) {}
CYassApp::~CYassApp() = default;

CYassApp* mApp;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }
  // Fix log output name
  SetExecutablePath(exec_path);

  if (!EnableSecureDllLoading()) {
    return -1;
  }

  // This function is primarily useful to applications that were linked with /SUBSYSTEM:WINDOWS,
  // which implies to the operating system that a console is not needed
  // before entering the program's main method.
  if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) != FILE_TYPE_UNKNOWN) {
    fprintf(stderr, "attached to current console\n");
    fflush(stderr);
  } else if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
    FILE* unusedFile;
    // Swap to the new out/err streams
    freopen_s(&unusedFile, "CONOUT$", "w", stdout);
    freopen_s(&unusedFile, "CONOUT$", "w", stderr);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    fprintf(stderr, "attached to parent process' console\n");
    fflush(stderr);
  }

  absl::InitializeSymbolizer(exec_path.c_str());
#ifdef HAVE_CRASHPAD
  CHECK(InitializeCrashpad(exec_path));
#else
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);
#endif

  // TODO move to standalone function
  // Parse command line for internal options
  // https://docs.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinew
  // The lifetime of the returned value is managed by the system, applications
  // should not free or modify this value.
  const wchar_t* cmdline = GetCommandLineW();
  int argc = 0;

  std::unique_ptr<wchar_t*[], decltype(&LocalFree)> wargv(CommandLineToArgvW(cmdline, &argc), &LocalFree);
  std::vector<std::string> argv_store(argc);
  std::vector<const char*> argv(argc, nullptr);
  for (int i = 0; i != argc; ++i) {
    argv_store[i] = SysWideToUTF8(wargv[i]);
    argv[i] = argv_store[i].data();
  }
  argv[0] = exec_path.data();

  config::SetClientUsageMessage(exec_path);
  config::ReadConfigFileAndArguments(argc, &argv[0]);

  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";

  CRYPTO_library_init();

  // TODO: transfer OutputDebugString to internal logging

  CYassApp app(hInstance);
  mApp = &app;

  return app.RunMainLoop();
}

BOOL CYassApp::InitInstance() {
  if (!CheckFirstInstance())
    return FALSE;

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

  frame_ = new CYassFrame;
  if (frame_ == nullptr) {
    return FALSE;
  }

  // https://docs.microsoft.com/en-us/windows/win32/menurc/using-menus
  const wchar_t* className = L"yassMainWnd";

  WNDCLASSEXW wndcls;

  // otherwise we need to register a new class
  wndcls.cbSize = sizeof(wndcls);
  wndcls.style = CS_DBLCLKS;
  wndcls.lpfnWndProc = &CYassFrame::WndProc;
  wndcls.cbClsExtra = wndcls.cbWndExtra = 0;
  wndcls.hInstance = m_hInstance;
  wndcls.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APPICON));
  wndcls.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wndcls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wndcls.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINFRAME);
  wndcls.lpszClassName = className;
  wndcls.hIconSm = nullptr;

  RegisterClassExW(&wndcls);

  std::wstring frame_name = LoadStringStdW(m_hInstance, IDS_APP_TITLE);

  UINT uDpi = Utils::GetDpiForWindowOrSystem(nullptr);
  RECT rect{0, 0, MULDIVDPI(530), MULDIVDPI(510)};

  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
  int nCmdShow = absl::GetFlag(FLAGS_background) ? SW_HIDE : SW_SHOW;
  if (!frame_->Create(className, frame_name.c_str(), WS_MINIMIZEBOX | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, rect,
                      m_hInstance, nCmdShow)) {
    LOG(WARNING) << "Failed to create main frame";
    delete frame_;
    return FALSE;
  }

  if (Utils::GetAutoStart()) {
    if (absl::GetFlag(FLAGS_background)) {
      DWORD main_thread_id = GetCurrentThreadId();
      WaitNetworkUp([=]() {
        bool ret;
        ret = PostThreadMessageW(main_thread_id, WM_MYAPP_NETWORK_UP, 0, 0);
        if (!ret) {
          PLOG(WARNING) << "Internal error: PostThreadMessage";
        }
      });
    } else {
      frame_->OnStartButtonClicked();
    }
  }

  return TRUE;
}

int CYassApp::ExitInstance() {
  LOG(WARNING) << "Application exiting";
  worker_.Stop([]() {});
  return 0;
}

int CYassApp::RunMainLoop() {
  MSG msg;

  HACCEL hAccelTable = LoadAcceleratorsW(m_hInstance, MAKEINTRESOURCEW(IDC_YASS));

  if (!InitInstance()) {
    return -1;
  }

  // Main message loop:
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagea
  for (;;) {
    for (;;) {
      // Wait until a message is available, up to the time needed by the timer
      // manager to fire the next set of timers.
      DWORD wait_flags = MWMO_INPUTAVAILABLE;
      // Tell the optimizer to retain these values to simplify analyzing hangs.
      Alias(&wait_flags);
      DWORD result = MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, wait_flags);

      if (result == WAIT_OBJECT_0) {
        // A WM_* message is available.
        if (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE) != FALSE)
          break;

        // We know there are no more messages for this thread because PeekMessage
        // has returned false. Reset |wait_flags| so that we wait for a *new*
        // message.
        wait_flags = 0;
      }

      DCHECK_NE(WAIT_FAILED, result) << GetLastError();
    }
    // ProcessNextWindowsMessage
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE) {
      if (msg.message == WM_QUIT)
        break;
      // The hwnd member of the returned MSG structure is NULL.
      if (msg.hwnd == nullptr)
        HandleThreadMessage(msg.message, msg.wParam, msg.lParam);
      if (frame_ && IsDialogMessageW(frame_->Wnd(), &msg))
        continue;
      if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    if (msg.message == WM_QUIT)
      break;
  }

  if (auto ret = ExitInstance()) {
    return ret;
  }

  PrintMallocStats();

  return static_cast<int>(msg.wParam);
}

// https://docs.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues
BOOL CYassApp::HandleThreadMessage(UINT message, WPARAM w, LPARAM l) {
  switch (message) {
    case WM_MYAPP_STARTED:
      OnStarted(w, l);
      return TRUE;
    case WM_MYAPP_START_FAILED:
      OnStartFailed(w, l);
      return TRUE;
    case WM_MYAPP_STOPPED:
      OnStopped(w, l);
      return TRUE;
    case WM_MYAPP_NETWORK_UP:
      if (state_ == STOPPED) {
        frame_->OnStartButtonClicked();
      }
      return TRUE;
  }
  return FALSE;
}

std::wstring CYassApp::GetStatus() const {
  std::wostringstream ss;
  if (state_ == STARTED) {
    ss << LoadStringStdW(m_hInstance, IDS_STATUS_CONNECTED_WITH_CONNS) << worker_.currentConnections();
  } else if (state_ == STARTING) {
    ss << LoadStringStdW(m_hInstance, IDS_STATUS_CONNECTING);
  } else if (state_ == START_FAILED) {
    ss << LoadStringStdW(m_hInstance, IDS_STATUS_FAILED_TO_CONNECT_DUE_TO) << SysUTF8ToWide(error_msg_);
  } else if (state_ == STOPPING) {
    ss << LoadStringStdW(m_hInstance, IDS_STATUS_DISCONNECTING);
  } else {
    ss << LoadStringStdW(m_hInstance, IDS_STATUS_DISCONNECTED_WITH) << SysUTF8ToWide(worker_.GetRemoteDomain());
  }
  return ss.str();
}

void CYassApp::OnStart(bool quiet) {
  DWORD main_thread_id = GetCurrentThreadId();
  state_ = STARTING;
  std::string err_msg = SaveConfig();
  if (!err_msg.empty()) {
    OnStartFailed(0, reinterpret_cast<LPARAM>(new std::string(err_msg)));
    return;
  }

  absl::AnyInvocable<void(asio::error_code)> callback;
  if (!quiet) {
    callback = [main_thread_id](asio::error_code ec) {
      bool successed = false;
      std::string* message = nullptr;
      bool ret;

      if (ec) {
        message = new std::string;
        *message = ec.message();
        successed = false;
      } else {
        successed = true;
      }

      // if the gui library exits, no more need to handle
      ret = PostThreadMessageW(main_thread_id, successed ? WM_MYAPP_STARTED : WM_MYAPP_START_FAILED, 0,
                               reinterpret_cast<LPARAM>(message));
      if (!ret) {
        PLOG(WARNING) << "Internal error: PostThreadMessage";
      }
    };
  }
  worker_.Start(std::move(callback));
}

void CYassApp::OnStop(bool quiet) {
  DWORD main_thread_id = GetCurrentThreadId();
  state_ = STOPPING;
  absl::AnyInvocable<void()> callback;
  if (!quiet) {
    callback = [main_thread_id]() {
      bool ret;
      ret = PostThreadMessageW(main_thread_id, WM_MYAPP_STOPPED, 0, 0);
      if (!ret) {
        PLOG(WARNING) << "Internal error: PostThreadMessage";
      }
    };
  }
  worker_.Stop(std::move(callback));
}

// https://docs.microsoft.com/en-us/windows/win32/winprog/windows-data-types?redirectedfrom=MSDN
void CYassApp::OnStarted(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STARTED;
  config::SaveConfig();
  frame_->OnStarted();
}

void CYassApp::OnStartFailed(WPARAM w, LPARAM l) {
  state_ = START_FAILED;
  std::unique_ptr<std::string> message_ptr(reinterpret_cast<std::string*>(l));

  error_msg_ = std::move(message_ptr.operator*());
  frame_->OnStartFailed();
}

void CYassApp::OnStopped(WPARAM /*w*/, LPARAM /*l*/) {
  state_ = STOPPED;
  frame_->OnStopped();
}

BOOL CYassApp::OnIdle() {
  return FALSE;
}

BOOL CYassApp::CheckFirstInstance() {
  std::wstring app_name = LoadStringStdW(m_hInstance, IDS_APP_TITLE);

  HWND first_wnd = FindWindowW(nullptr, app_name.c_str());

  // another instance is already running - activate it
  if (first_wnd) {
    HWND popup_wnd = GetLastActivePopup(first_wnd);
    SetForegroundWindow(popup_wnd);
    if (!IsWindowVisible(popup_wnd))
      ShowWindow(popup_wnd, SW_SHOW);
    if (IsIconic(popup_wnd))
      ShowWindow(popup_wnd, SW_SHOWNORMAL);
    if (first_wnd != popup_wnd)
      SetForegroundWindow(popup_wnd);
    return FALSE;
  }

  // this is the first instance
  return TRUE;
}

std::string CYassApp::SaveConfig() {
  auto server_host = frame_->GetServerHost();
  auto server_sni = frame_->GetServerSNI();
  auto server_port = frame_->GetServerPort();
  auto username = frame_->GetUsername();
  auto password = frame_->GetPassword();
  auto method = frame_->GetMethod();
  auto local_host = frame_->GetLocalHost();
  auto local_port = frame_->GetLocalPort();
  auto doh_url = frame_->GetDoHURL();
  auto dot_host = frame_->GetDoTHost();
  auto connect_timeout = frame_->GetTimeout();

  return config::ReadConfigFromArgument(server_host, server_sni, server_port, username, password, method, local_host,
                                        local_port, doh_url, dot_host, connect_timeout);
}
