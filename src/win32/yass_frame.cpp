// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "win32/yass_frame.hpp"

#include <absl/flags/flag.h>
#include <iomanip>
#include <sstream>

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include "cli/cli_connection_stats.hpp"
#include "core/utils.hpp"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass.hpp"

// below definition comes from WinUser.h
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

// https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winui/shell/appshellintegration/NotificationIcon/NotificationIcon.cpp
#define TRAY_ICON_ID 0x100
// Use a guid to uniquely identify our icon
#ifdef COMPILER_MSVC
class __declspec(uuid("4324603D-4274-47AA-BAD5-7CF638A863C6")) TrayIcon;
#else
DEFINE_GUID(CLSID_TrayIcon, 0x4324603D, 0x4274, 0x47AA, 0xBA, 0xD5, 0x7C, 0xF6, 0x38, 0xA8, 0x63, 0xC6);
class DECLSPEC_UUID("4324603D-4274-47AA-BAD5-7CF638A863C6") TrayIcon;
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(TrayIcon, 0x4324603D, 0x4274, 0x47AA, 0xBA, 0xD5, 0x7C, 0xF6, 0x38, 0xA8, 0x63, 0xC6)
#endif
#endif

#define INITIAL_COLUMN_ONE_LEFT 20
#define INITIAL_COLUMN_TWO_LEFT 120
#define INITIAL_COLUMN_THREE_LEFT 240

#define INITIAL_VERTICAL_HEIGHT 30

#define INITIAL_BUTTON_WIDTH 75
#define INITIAL_BUTTON_HEIGHT 30

#define INITIAL_LABEL_WIDTH 100
#define INITIAL_LABEL_HEIGHT 25
#define INITIAL_EDIT_WIDTH 220
#define INITIAL_EDIT_HEIGHT 25
#define INITIAL_STATUS_BAR_HEIGHT 20

#define COLUMN_ONE_LEFT MulDiv(INITIAL_COLUMN_ONE_LEFT, uDpi, 96)
#define COLUMN_TWO_LEFT MulDiv(INITIAL_COLUMN_TWO_LEFT, uDpi, 96)
#define COLUMN_THREE_LEFT MulDiv(INITIAL_COLUMN_THREE_LEFT, uDpi, 96)

#define VERTICAL_HEIGHT MulDiv(INITIAL_VERTICAL_HEIGHT, uDpi, 96)

#define BUTTON_WIDTH MulDiv(INITIAL_BUTTON_WIDTH, uDpi, 96)
#define BUTTON_HEIGHT MulDiv(INITIAL_BUTTON_HEIGHT, uDpi, 96)

#define LABEL_WIDTH MulDiv(INITIAL_LABEL_WIDTH, uDpi, 96)
#define LABEL_HEIGHT MulDiv(INITIAL_LABEL_HEIGHT, uDpi, 96)
#define EDIT_WIDTH MulDiv(INITIAL_EDIT_WIDTH, uDpi, 96)
#define EDIT_HEIGHT MulDiv(INITIAL_EDIT_HEIGHT, uDpi, 96)

#define STATUS_BAR_HEIGHT MulDiv(INITIAL_STATUS_BAR_HEIGHT, uDpi, 96)

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2)
      << static_cast<double>(value) / 1024.0 << " " << *c;
}

static std::string GetWindowTextStd(HWND hWnd) {
  std::wstring text;
  int len = GetWindowTextLengthW(hWnd);
  text.resize(len);
  GetWindowTextW(hWnd, const_cast<wchar_t*>(text.c_str()), len + 1);
  return SysWideToUTF8(text);
}

static void SetWindowTextStd(HWND hWnd, const std::string& text) {
  SetWindowTextW(hWnd, SysUTF8ToWide(text).c_str());
}

namespace {
HWND CreateStatic(const wchar_t* label,
                  HWND pParentWnd,
                  UINT nID,
                  HINSTANCE hInstance) {
  return CreateWindowExW(
      0, WC_STATICW, label, WS_CHILD | WS_VISIBLE | SS_LEFT,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      pParentWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), hInstance, nullptr);
}

HWND CreateEdit(DWORD dwStyle,
                HWND pParentWnd,
                UINT nID,
                HINSTANCE hInstance) {
  return CreateWindowExW(
      0, WC_EDITW, nullptr,
      WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | WS_BORDER | ES_LEFT | dwStyle,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      pParentWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), hInstance, nullptr);
}

HWND CreateComboBox(DWORD dwStyle,
                    HWND pParentWnd,
                    UINT nID,
                    HINSTANCE hInstance) {
  return CreateWindowExW(
      0, WC_COMBOBOXW, nullptr,
      WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_VSCROLL | dwStyle,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      pParentWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), hInstance, nullptr);
}

HWND CreateButton(const wchar_t* label,
                  DWORD dwStyle,
                  HWND pParentWnd,
                  UINT nID,
                  HINSTANCE hInstance) {
  return CreateWindowExW(
      0, WC_BUTTONW, label,
      WS_TABSTOP | WS_CHILD | WS_VISIBLE | dwStyle,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      pParentWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), hInstance, nullptr);
}

HWND CreateStatusBar(HWND pParentWnd,
                     int idStatus,
                     HINSTANCE hInstance,
                     int cParts) {
  HWND hWnd;
  RECT rcClient;
  HLOCAL hloc;
  PINT paParts;
  int i, nWidth;

  // Create the status bar.
  hWnd = CreateWindowEx(0,                      // no extended styles
                        STATUSCLASSNAME,        // name of status bar class
                        nullptr,                // no text when first created
                        WS_CHILD | WS_VISIBLE,  // creates a visible child window
                        0, 0, 0, 0,             // ignores size and position
                        pParentWnd,             // handle to parent window
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(idStatus)),  // child window identifier
                        hInstance,  // handle to application instance
                        nullptr);   // no window creation data

  // Get the coordinates of the parent window's client area.
  GetClientRect(pParentWnd, &rcClient);

  // Allocate an array for holding the right edge coordinates.
  hloc = LocalAlloc(LHND, sizeof(int) * cParts);
  paParts = static_cast<PINT>(LocalLock(hloc));

  // Calculate the right edge coordinate for each part, and
  // copy the coordinates to the array.
  nWidth = rcClient.right / cParts;
  int rightEdge = nWidth;
  for (i = 0; i < cParts; i++) {
    paParts[i] = rightEdge;
    rightEdge += nWidth;
  }

  // Tell the status bar to create the window parts.
  SendMessage(hWnd, SB_SETPARTS, static_cast<WPARAM>(cParts), reinterpret_cast<LPARAM>(paParts));

  // Free the array, and return.
  LocalUnlock(hloc);
  LocalFree(hloc);
  return hWnd;
}

BOOL AddNotificationIcon(HWND hwnd, HINSTANCE hInstance) {
  // Add Notification Icon
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  wcscpy(nid.szTip, L"Show YASS");
#if _WIN32_WINNT >= 0x0600
  nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
  nid.guidItem = __uuidof(TrayIcon);
  LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_TRAYICON), LIM_SMALL, &nid.hIcon);
#else
  nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
  nid.uID = TRAY_ICON_ID;
  nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYICON));
#endif
  nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
  Shell_NotifyIcon(NIM_ADD, &nid);
#if _WIN32_WINNT >= 0x0600
  // NOTIFICATION_VERSION_4 is perfered
  nid.uVersion = NOTIFYICON_VERSION_4;
  return Shell_NotifyIcon(NIM_SETVERSION, &nid);
#else
  return TRUE;
#endif
}

BOOL UpdateNotificationIcon(HINSTANCE hInstance, UINT uDpi) {
#if _WIN32_WINNT >= 0x0600
  // Add Notification Icon
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.uFlags = NIF_ICON | NIF_GUID;
  nid.guidItem = __uuidof(TrayIcon);
  LoadIconMetric(hInstance, MAKEINTRESOURCE(IDI_TRAYICON),
                 uDpi > 96 ? LIM_LARGE : LIM_SMALL, &nid.hIcon);
  return Shell_NotifyIcon(NIM_MODIFY, &nid);
#else
  return TRUE;
#endif
}

BOOL DeleteNotificationIcon(HWND hwnd) {
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
#if _WIN32_WINNT >= 0x0600
  nid.uFlags = NIF_GUID;
  nid.guidItem = __uuidof(TrayIcon);
#else
  nid.uID = TRAY_ICON_ID;
#endif
  return Shell_NotifyIcon(NIM_DELETE, &nid);
}

BOOL RestoreTooltip() {
#if _WIN32_WINNT >= 0x0600
  // After the balloon is dismissed, restore the tooltip.
  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(nid);
  nid.uFlags = NIF_SHOWTIP | NIF_GUID;
  nid.guidItem = __uuidof(TrayIcon);
  return Shell_NotifyIcon(NIM_MODIFY, &nid);
#else
  return TRUE;
#endif
}

void ShowContextMenu(HINSTANCE hInstance, HWND hwnd, POINT pt) {
  HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDC_CONTEXTMENU));
  if (hMenu) {
    HMENU hSubMenu = GetSubMenu(hMenu, 0);
    if (hSubMenu) {
      // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
      SetForegroundWindow(hwnd);

      // respect menu drop alignment
      UINT uFlags = TPM_RIGHTBUTTON;
      if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
        uFlags |= TPM_RIGHTALIGN;
      } else {
        uFlags |= TPM_LEFTALIGN;
      }

      TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, nullptr);
    }
    DestroyMenu(hMenu);
  }
}

} // namespace

static CYassFrame* mFrame;

CYassFrame::CYassFrame() = default;
CYassFrame::~CYassFrame() = default;

int CYassFrame::Create(const wchar_t* className,
                       const wchar_t* title,
                       DWORD dwStyle,
                       RECT rect,
                       HINSTANCE hInstance,
                       int nCmdShow) {
  // FIXME
  mFrame = this;

  m_hInstance = hInstance;
  m_hWnd = CreateWindowExW(0, className, title, dwStyle, rect.left, rect.top,
                           rect.right - rect.left, rect.bottom - rect.top,
                           nullptr, nullptr, hInstance, nullptr);

  SetWindowLongPtrW(m_hWnd, GWLP_HINSTANCE, reinterpret_cast<LPARAM>(hInstance));

  rect = RECT{};

  // Left Panel
  start_button_ = CreateButton(L"START", BS_PUSHBUTTON, m_hWnd, IDC_START, hInstance);

  stop_button_ = CreateButton(L"STOP", BS_PUSHBUTTON, m_hWnd, IDC_STOP, hInstance);

  EnableWindow(stop_button_, FALSE);

  // Right Panel
  const wchar_t* method_strings[] = {
#define XX(num, name, string) L##string,
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  DWORD method_nums[] = {
#define XX(num, name, string) static_cast<DWORD>(num),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  // https://docs.microsoft.com/en-us/windows/win32/controls/individual-control-info
  // https://docs.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
  // https://docs.microsoft.com/en-us/windows/win32/winmsg/window-styles
  // https://docs.microsoft.com/en-us/windows/win32/winmsg/about-window-classes
  // Right Panel
  // Column 2
  server_host_label_ = CreateStatic(L"Server Host", m_hWnd, 0, hInstance);
  server_port_label_ = CreateStatic(L"Server Port", m_hWnd, 0, hInstance);
  username_label_ = CreateStatic(L"Username", m_hWnd, 0, hInstance);
  password_label_ = CreateStatic(L"Password", m_hWnd, 0, hInstance);
  method_label_ = CreateStatic(L"Cipher Method", m_hWnd, 0, hInstance);
  local_host_label_ = CreateStatic(L"Local Host", m_hWnd, 0, hInstance);
  local_port_label_ = CreateStatic(L"Local Port", m_hWnd, 0, hInstance);
  timeout_label_ = CreateStatic(L"Timeout", m_hWnd, 0, hInstance);
  autostart_label_ = CreateStatic(L"Auto Start", m_hWnd, 0, hInstance);
  systemproxy_label_ = CreateStatic(L"System Proxy", m_hWnd, 0, hInstance);

  // Column 3
  server_host_edit_ = CreateEdit(0, m_hWnd, IDC_EDIT_SERVER_HOST, hInstance);
  server_port_edit_ = CreateEdit(ES_NUMBER, m_hWnd, IDC_EDIT_SERVER_PORT, hInstance);
  username_edit_ = CreateEdit(0, m_hWnd, IDC_EDIT_USERNAME, hInstance);
  password_edit_ = CreateEdit(ES_PASSWORD, m_hWnd, IDC_EDIT_PASSWORD, hInstance);

  method_combo_box_ = CreateComboBox(CBS_DROPDOWNLIST, m_hWnd,
                                     IDC_COMBOBOX_METHOD, hInstance);

  int method_count = sizeof(method_strings) / sizeof(method_strings[0]) - 1;
  for (int i = 0; i < method_count; ++i) {
    ComboBox_AddString(method_combo_box_, method_strings[i + 1]);
    ComboBox_SetItemData(method_combo_box_, i, method_nums[i + 1]);
  }

  ComboBox_SetMinVisible(method_combo_box_, method_count);

  local_host_edit_ = CreateEdit(0, m_hWnd, IDC_EDIT_LOCAL_HOST, hInstance);
  local_port_edit_ = CreateEdit(ES_NUMBER, m_hWnd, IDC_EDIT_LOCAL_PORT, hInstance);
  timeout_edit_ = CreateEdit(ES_NUMBER, m_hWnd, IDC_EDIT_TIMEOUT, hInstance);

  autostart_button_ = CreateButton(L"Enable", BS_AUTOCHECKBOX | BS_LEFT,
                                   m_hWnd, IDC_AUTOSTART_CHECKBOX, hInstance);

  systemproxy_button_ = CreateButton(L"Enable", BS_AUTOCHECKBOX | BS_LEFT,
                                     m_hWnd, IDC_SYSTEMPROXY_CHECKBOX, hInstance);

  Button_SetCheck(autostart_button_,
                  Utils::GetAutoStart() ? BST_CHECKED : BST_UNCHECKED);

  Button_SetCheck(systemproxy_button_,
                  Utils::GetSystemProxy() ? BST_CHECKED : BST_UNCHECKED);

  // Status Bar
  // https://docs.microsoft.com/en-us/windows/win32/controls/status-bars
  status_bar_ = CreateStatusBar(m_hWnd, ID_APP_MSG, hInstance, 1);

  if (!start_button_ || !stop_button_ ||
      !server_host_label_|| !server_port_label_||
      !username_label_|| !password_label_||
      !method_label_||
      !local_host_label_|| !local_port_label_||
      !timeout_label_|| !autostart_label_||
      !server_host_edit_|| !server_port_edit_||
      !username_edit_|| !password_edit_||
      !method_combo_box_||
      !local_host_edit_|| !local_port_edit_||
      !timeout_edit_|| !autostart_button_|| !status_bar_)
    return FALSE;

  UpdateLayoutForDpi();

  LoadConfig();

  CentreWindow();
  ShowWindow(m_hWnd, SW_SHOW);
  if (nCmdShow != SW_SHOW) {
    ShowWindow(m_hWnd, nCmdShow);
  }
  UpdateWindow(m_hWnd);

  SetTimer(m_hWnd, IDT_UPDATE_STATUS_BAR, 200, nullptr);

  return TRUE;
}

void CYassFrame::CentreWindow() {
  MONITORINFO mi;
  mi.cbSize = sizeof(mi);
  RECT rcDlg, rcCentre, rcArea;

  // get coordinates of the window relative to its parent
  GetWindowRect(m_hWnd, &rcDlg);

  // center within appropriate monitor coordinates
  GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi);
  rcCentre = mi.rcWork;
  rcArea = mi.rcWork;

  // find dialog's upper left based on rcCenter
  int xLeft = (rcCentre.left + rcCentre.right) / 2 - (rcDlg.right - rcDlg.left) / 2;
  int yTop = (rcCentre.top + rcCentre.bottom) / 2 - (rcDlg.bottom - rcDlg.top) / 2;

  // if the dialog is outside the screen, move it inside
  if (xLeft + (rcDlg.right - rcDlg.left) > rcArea.right)
    xLeft = rcArea.right - (rcDlg.right - rcDlg.left);
  if (xLeft < rcArea.left)
    xLeft = rcArea.left;

  if (yTop + (rcDlg.bottom - rcDlg.top) > rcArea.bottom)
    yTop = rcArea.bottom - (rcDlg.bottom - rcDlg.top);
  if (yTop < rcArea.top)
    yTop = rcArea.top;

  // map screen coordinates to child coordinates
  SetWindowPos(m_hWnd, nullptr, xLeft, yTop, -1, -1,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// https://docs.microsoft.com/en-us/windows/win32/winmsg/window-notifications
// http://msdn.microsoft.com/en-us/library/ms700677(v=vs.85).aspx
//
// https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows?redirectedfrom=MSDN
// https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-reference
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-getdpiscaledsize
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-beforeparent
// https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-afterparent
// static
LRESULT CALLBACK CYassFrame::WndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      AddNotificationIcon(hWnd, mFrame->m_hInstance);
      break;
    case WM_NCCREATE: {
      // Enable per-monitor DPI scaling for caption, menu, and top-level
      // scroll bars.
      //
      // Non-client area (scroll bars, caption bar, etc.) does not DPI scale
      // automatically on Windows 8.1. In Windows 10 (1607) support was added
      // for this via a call to EnableNonClientDpiScaling. Windows 10 (1703)
      // supports this automatically when the DPI_AWARENESS_CONTEXT is
      // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2.
      if (!Utils::EnableNonClientDpiScaling(hWnd)) {
        PLOG(WARNING) << "Internal error: EnableNonClientDpiScaling failed";
      }

      return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    case WM_CLOSE:
      mFrame->OnClose();
      break;
    case WM_QUERYENDSESSION:
      return static_cast<INT_PTR>(mFrame->OnQueryEndSession());
    case WM_DESTROY:
      DeleteNotificationIcon(mFrame->m_hWnd);
      PostQuitMessage(0);
      break;
    case WM_DPICHANGED:
      return mFrame->OnDPIChanged(wParam, lParam);
    case WM_COMMAND: {
      int wmId = LOWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
      case ID_APP_OPTION:
        mFrame->OnAppOption();
        break;
      case ID_APP_ABOUT:
        mFrame->OnAppAbout();
        break;
      case ID_APP_EXIT:
        mFrame->OnClose();
        break;
      // https://docs.microsoft.com/en-us/windows/win32/controls/bn-clicked
      case IDC_START:
        mFrame->OnStartButtonClicked();
        break;
      case IDC_STOP:
        mFrame->OnStopButtonClicked();
        break;
      case IDC_AUTOSTART_CHECKBOX:
        mFrame->OnCheckedAutoStartButtonClicked();
        break;
      case IDC_SYSTEMPROXY_CHECKBOX:
        mFrame->OnCheckedSystemProxyButtonClicked();
        break;
      default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
      }
      break;
    }
    case WM_TIMER:
      switch (wParam) {
        case IDT_UPDATE_STATUS_BAR:
          mFrame->OnUpdateStatusBar();
          break;
        default:
          return DefWindowProc(hWnd, msg, wParam, lParam);
      }
      break;
    case WMAPP_NOTIFYCALLBACK: {
        switch(LOWORD(lParam)) {
#if _WIN32_WINNT >= 0x0600
        case NIN_SELECT:
#else
        case WM_LBUTTONUP:
#endif
          // for NOTIFYICON_VERSION_4 clients, NIN_SELECT is prerable to listening to mouse clicks and key presses
          // directly.
          if (IsWindowVisible(mFrame->m_hWnd)) {
            ShowWindow(mFrame->m_hWnd, SW_HIDE);
          } else {
            ShowWindow(mFrame->m_hWnd, SW_SHOW);
          }
          break;

        case NIN_BALLOONTIMEOUT:
          RestoreTooltip();
          break;

        case NIN_BALLOONUSERCLICK:
          RestoreTooltip();
          break;

#if _WIN32_WINNT >= 0x0600
        case WM_CONTEXTMENU:
          {
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
#else
        case WM_RBUTTONUP:
          {
            POINT pt;
            GetCursorPos(&pt);
#endif
            ShowContextMenu(mFrame->m_hInstance, mFrame->m_hWnd, pt);
          }
          break;
        }
        break;
      break;
    }
    default:
      return DefWindowProc(hWnd, msg, wParam, lParam);
  }
  return 0;
}

std::string CYassFrame::GetServerHost() {
  return GetWindowTextStd(server_host_edit_);
}

std::string CYassFrame::GetServerPort() {
  return GetWindowTextStd(server_port_edit_);
}

std::string CYassFrame::GetUsername() {
  return GetWindowTextStd(username_edit_);
}

std::string CYassFrame::GetPassword() {
  return GetWindowTextStd(password_edit_);
}

cipher_method CYassFrame::GetMethod() {
  int method = ComboBox_GetItemData(method_combo_box_,
                                    ComboBox_GetCurSel(method_combo_box_));
  return static_cast<enum cipher_method>(method);
}

std::string CYassFrame::GetLocalHost() {
  return GetWindowTextStd(local_host_edit_);
}

std::string CYassFrame::GetLocalPort() {
  return GetWindowTextStd(local_port_edit_);
}

std::string CYassFrame::GetTimeout() {
  return GetWindowTextStd(timeout_edit_);
}

std::wstring CYassFrame::GetStatusMessage() {
  if (mApp->GetState() != CYassApp::STARTED) {
    return SysUTF8ToWide(mApp->GetStatus());
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = cli::total_rx_bytes;
    uint64_t tx_bytes = cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) /
               static_cast<double>(delta_time) * NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) /
               static_cast<double>(delta_time) * NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  ss << mApp->GetStatus();
  ss << " tx rate: ";
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << " rx rate: ";
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return SysUTF8ToWide(ss.str());
}

void CYassFrame::OnStarted() {
  EnableWindow(server_host_edit_, FALSE);
  EnableWindow(server_port_edit_, FALSE);
  EnableWindow(username_edit_, FALSE);
  EnableWindow(password_edit_, FALSE);
  EnableWindow(method_combo_box_, FALSE);
  EnableWindow(local_host_edit_, FALSE);
  EnableWindow(local_port_edit_, FALSE);
  EnableWindow(timeout_edit_, FALSE);
  EnableWindow(autostart_button_, FALSE);
  EnableWindow(stop_button_, TRUE);
}

void CYassFrame::OnStartFailed() {
  EnableWindow(server_host_edit_, TRUE);
  EnableWindow(server_port_edit_, TRUE);
  EnableWindow(username_edit_, TRUE);
  EnableWindow(password_edit_, TRUE);
  EnableWindow(method_combo_box_, TRUE);
  EnableWindow(local_host_edit_, TRUE);
  EnableWindow(local_port_edit_, TRUE);
  EnableWindow(timeout_edit_, TRUE);
  EnableWindow(autostart_button_, TRUE);
  EnableWindow(start_button_, TRUE);
  MessageBoxW(m_hWnd, SysUTF8ToWide(mApp->GetStatus()).c_str(), L"Start Failed",
              MB_ICONEXCLAMATION | MB_OK);
}

void CYassFrame::OnStopped() {
  EnableWindow(server_host_edit_, TRUE);
  EnableWindow(server_port_edit_, TRUE);
  EnableWindow(username_edit_, TRUE);
  EnableWindow(password_edit_, TRUE);
  EnableWindow(method_combo_box_, TRUE);
  EnableWindow(local_host_edit_, TRUE);
  EnableWindow(local_port_edit_, TRUE);
  EnableWindow(timeout_edit_, TRUE);
  EnableWindow(autostart_button_, TRUE);
  EnableWindow(start_button_, TRUE);
}

void CYassFrame::LoadConfig() {
  std::string server_host(absl::GetFlag(FLAGS_server_host));
  std::string server_port(std::to_string(absl::GetFlag(FLAGS_server_port)));
  std::string username(absl::GetFlag(FLAGS_username));
  std::string password(absl::GetFlag(FLAGS_password));
  std::string local_host(absl::GetFlag(FLAGS_local_host));
  std::string local_port(std::to_string(absl::GetFlag(FLAGS_local_port)));
  std::string timeout(std::to_string(absl::GetFlag(FLAGS_connect_timeout)));

  SetWindowTextStd(server_host_edit_, server_host);
  SetWindowTextStd(server_port_edit_, server_port);
  SetWindowTextStd(username_edit_, username);
  SetWindowTextStd(password_edit_, password);

  int32_t method = absl::GetFlag(FLAGS_method).method;
  for (int i = 0, cnt = ComboBox_GetCount(method_combo_box_); i < cnt; ++i) {
    if (ComboBox_GetItemData(method_combo_box_, i) ==
        static_cast<LRESULT>(method)) {
      ComboBox_SetCurSel(method_combo_box_, i);
      break;
    }
  }

  SetWindowTextStd(local_host_edit_, local_host);
  SetWindowTextStd(local_port_edit_, local_port);
  SetWindowTextStd(timeout_edit_, timeout);
}

void CYassFrame::UpdateLayoutForDpi() {
  UINT uDPI = Utils::GetDpiForWindowOrSystem(m_hWnd);
  LOG(WARNING) << "Adjust layout to Dpi: " << uDPI;
  UpdateLayoutForDpi(uDPI);
}

void CYassFrame::UpdateLayoutForDpi(UINT uDpi) {
  // Left Panel
  RECT rect, client_rect;

  GetClientRect(m_hWnd, &client_rect);

  rect.left = client_rect.left + COLUMN_ONE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT;

  SetWindowPos(start_button_, nullptr, rect.left, rect.top, BUTTON_WIDTH,
               BUTTON_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;

  rect.left = client_rect.left + COLUMN_ONE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 5;
  SetWindowPos(stop_button_, nullptr, rect.left, rect.top, BUTTON_WIDTH,
               BUTTON_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  // RIGHT Panel
  // Column 2
  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT;
  SetWindowPos(server_host_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 2;
  SetWindowPos(server_port_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 3;
  SetWindowPos(username_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 4;
  SetWindowPos(password_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 5;
  SetWindowPos(method_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 6;
  SetWindowPos(local_host_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 7;
  SetWindowPos(local_port_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 8;
  SetWindowPos(timeout_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 9;
  SetWindowPos(autostart_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_TWO_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 10;
  SetWindowPos(systemproxy_label_, nullptr, rect.left, rect.top, LABEL_WIDTH,
               LABEL_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  // Column 3
  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT;
  SetWindowPos(server_host_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 2;
  SetWindowPos(server_port_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 3;
  SetWindowPos(username_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 4;
  SetWindowPos(password_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 5;
  SetWindowPos(method_combo_box_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 6;
  SetWindowPos(local_host_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 7;
  SetWindowPos(local_port_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 8;
  SetWindowPos(timeout_edit_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 9;
  SetWindowPos(autostart_button_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  rect.left = client_rect.left + COLUMN_THREE_LEFT;
  rect.top = client_rect.top + VERTICAL_HEIGHT * 10;
  SetWindowPos(systemproxy_button_, nullptr, rect.left, rect.top, EDIT_WIDTH,
               EDIT_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

  // Status Bar
  RECT rcClient, status_bar_rect;
  GetClientRect(m_hWnd, &rcClient);
  GetClientRect(status_bar_, &status_bar_rect);
  status_bar_rect.top = rcClient.top - STATUS_BAR_HEIGHT;
  status_bar_rect.bottom = rcClient.bottom;
  SetWindowPos(status_bar_, nullptr,
               status_bar_rect.left, status_bar_rect.top,
               status_bar_rect.right - status_bar_rect.left,
               status_bar_rect.bottom - status_bar_rect.top,
               SWP_NOZORDER | SWP_NOACTIVATE);
}

void CYassFrame::OnClose() {
  LOG(WARNING) << "Frame is closing";
  KillTimer(m_hWnd, IDT_UPDATE_STATUS_BAR);
  DestroyWindow(m_hWnd);
}

BOOL CYassFrame::OnQueryEndSession() {
  LOG(WARNING) << "Frame is closing";
  if (mApp->GetState() == CYassApp::STARTED) {
    OnStopButtonClicked();
  }
  // If we are sure to block shutdown in Windows Vista or later:
  // When your application needs to block shutdown, it should call
  // ShutdownBlockReasonCreate() to register a reason string, and pass in a
  // handle to the window it uses to handle WM_QUERYENDSESSION.
  // When your application no longer needs to block shutdown, it should call
  // ShutdownBlockReasonDestroy() to unregister its reason string.
  // If your application needs to determine what reason string it registered
  // earlier, it should call ShutdownBlockReasonQuery() to retrieve it.

  // Applications that return TRUE to WM_QUERYENDSESSION will be closed at
  // shutdown
  return TRUE;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/bumper-status-bars-reference-messages
void CYassFrame::OnUpdateStatusBar() {
  if (IsIconic(m_hWnd))
    return;
  std::wstring status_text = GetStatusMessage();
  if (previous_status_message_ == status_text)
    return;
  previous_status_message_ = status_text;
  SendMessage(status_bar_, SB_SETTEXT,
              /* WPARAM */0, reinterpret_cast<LPARAM>(status_text.c_str()));
  UpdateWindow(status_bar_);
}

LRESULT CYassFrame::OnDPIChanged(WPARAM w, LPARAM l) {
  LOG(WARNING) << "DPI changed";

  // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DPIAwarenessPerWindow/client/DpiAwarenessContext.cpp
  UINT uDpi = HIWORD(w);

  // Resize the window
  auto lprcNewScale = reinterpret_cast<RECT*>(l);

  SetWindowPos(m_hWnd, nullptr, lprcNewScale->left, lprcNewScale->top,
               lprcNewScale->right - lprcNewScale->left,
               lprcNewScale->bottom - lprcNewScale->top,
               SWP_NOZORDER | SWP_NOACTIVATE);

  UpdateLayoutForDpi(uDpi);
  UpdateNotificationIcon(m_hInstance, uDpi);
  return 0;
}

void CYassFrame::OnStartButtonClicked() {
  EnableWindow(start_button_, FALSE);
  mApp->OnStart();
}

void CYassFrame::OnStopButtonClicked() {
  EnableWindow(stop_button_, FALSE);
  mApp->OnStop();
}

void CYassFrame::OnCheckedAutoStartButtonClicked() {
  Utils::EnableAutoStart(Button_GetCheck(autostart_button_) & BST_CHECKED);
}

void CYassFrame::OnCheckedSystemProxyButtonClicked() {
  Utils::SetSystemProxy(Button_GetCheck(systemproxy_button_) & BST_CHECKED);
}

void CYassFrame::OnAppOption() {
  DialogBoxParamW(m_hInstance, MAKEINTRESOURCE(IDD_OPTIONBOX), m_hWnd,
                  &CYassFrame::OnAppOptionMessage,
                  reinterpret_cast<LPARAM>(this));
}

// static
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-dlgproc
INT_PTR CALLBACK CYassFrame::OnAppOptionMessage(HWND hDlg, UINT message,
                                                WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);

  switch (message) {
    case WM_INITDIALOG: {
      SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
      // extra initialization to all fields
      auto tcp_keep_alive = absl::GetFlag(FLAGS_tcp_keep_alive);
      auto tcp_keep_alive_timeout = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
      auto tcp_keep_alive_interval = absl::GetFlag(FLAGS_tcp_keep_alive_interval);
      CheckDlgButton(hDlg, IDC_CHECKBOX_TCP_KEEP_ALIVE,
                     tcp_keep_alive ? BST_CHECKED : BST_UNCHECKED);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_KEEP_ALIVE_TIMEOUT, tcp_keep_alive_timeout,
                    FALSE);
      SetDlgItemInt(hDlg, IDC_EDIT_TCP_KEEP_ALIVE_INTERVAL, tcp_keep_alive_interval,
                    FALSE);
      return static_cast<INT_PTR>(TRUE);
    }
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK) {
        BOOL translated;
        // TODO prompt a fix-me tip
        auto tcp_keep_alive = IsDlgButtonChecked(hDlg, IDC_CHECKBOX_TCP_KEEP_ALIVE) == BST_CHECKED;
        auto tcp_keep_alive_timeout = GetDlgItemInt(
            hDlg, IDC_EDIT_TCP_KEEP_ALIVE_TIMEOUT, &translated, FALSE);
        if (translated == FALSE)
          return static_cast<INT_PTR>(FALSE);
        auto tcp_keep_alive_interval = GetDlgItemInt(
            hDlg, IDC_EDIT_TCP_KEEP_ALIVE_INTERVAL, &translated, FALSE);
        if (translated == FALSE)
          return static_cast<INT_PTR>(FALSE);
        absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
        absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_timeout);
        absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval);
      }
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return static_cast<INT_PTR>(TRUE);
      }
      break;
    default:
      break;
  }
  return static_cast<INT_PTR>(FALSE);
}

void CYassFrame::OnAppAbout() {
  DialogBoxParamW(m_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), m_hWnd,
                  &CYassFrame::OnAppAboutMessage, 0L);
}

// static
INT_PTR CALLBACK CYassFrame::OnAppAboutMessage(HWND hDlg, UINT message,
                                               WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return static_cast<INT_PTR>(TRUE);
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK) {
        EndDialog(hDlg, LOWORD(wParam));
        return static_cast<INT_PTR>(TRUE);
      }
      break;
    default:
      break;
  }
  return static_cast<INT_PTR>(FALSE);
}

