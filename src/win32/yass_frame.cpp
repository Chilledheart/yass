// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "win32/yass_frame.hpp"

#include <absl/flags/flag.h>
#include <iomanip>
#include <sstream>

#include "cli/socks5_connection_stats.hpp"
#include "core/utils.hpp"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass.hpp"

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

static void humanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (unsigned i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2)
      << static_cast<double>(value) / 1024.0 << " " << *c;
}

static UINT BASED_CODE indicators[] = {
    ID_APP_MSG,
};

IMPLEMENT_DYNCREATE(CYassFrame, CFrameWnd)

CYassFrame::CYassFrame() = default;
CYassFrame::~CYassFrame() = default;

BEGIN_MESSAGE_MAP(CYassFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_CLOSE()
  // https://docs.microsoft.com/en-us/cpp/mfc/on-update-command-ui-macro?view=msvc-170
  ON_UPDATE_COMMAND_UI(ID_APP_MSG, &CYassFrame::OnUpdateStatusBar)
#if defined(_WIN32_WINNT_WINBLUE) && _WIN32_WINNT >= _WIN32_WINNT_WINBLUE
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows?redirectedfrom=MSDN
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-reference
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-getdpiscaledsize
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-beforeparent
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-afterparent
  ON_MESSAGE(WM_DPICHANGED, &CYassFrame::OnDPIChanged)
#endif
  ON_BN_CLICKED(IDC_START, &CYassFrame::OnStartButtonClicked)
  ON_BN_CLICKED(IDC_STOP, &CYassFrame::OnStopButtonClicked)
  ON_BN_CLICKED(IDC_AUTOSTART_CHECKBOX,
                &CYassFrame::OnCheckedAutoStartButtonClicked)
END_MESSAGE_MAP()

std::string CYassFrame::GetServerHost() {
  CString server_host;
  serverhost_edit_.GetWindowText(server_host);
  return SysWideToUTF8(server_host.operator const wchar_t*());
}

std::string CYassFrame::GetServerPort() {
  CString server_port;
  serverport_edit_.GetWindowText(server_port);
  return SysWideToUTF8(server_port.operator const wchar_t*());
}

std::string CYassFrame::GetPassword() {
  CString password;
  password_edit_.GetWindowText(password);
  return SysWideToUTF8(password.operator const wchar_t*());
}

cipher_method CYassFrame::GetMethod() {
  int method = static_cast<int>(
      method_combo_box_.GetItemData(method_combo_box_.GetCurSel()));
  return static_cast<enum cipher_method>(method);
}

std::string CYassFrame::GetLocalHost() {
  CString local_host;
  localhost_edit_.GetWindowText(local_host);
  return SysWideToUTF8(local_host.operator const wchar_t*());
}

std::string CYassFrame::GetLocalPort() {
  CString local_port;
  localport_edit_.GetWindowText(local_port);
  return SysWideToUTF8(local_port.operator const wchar_t*());
}

std::string CYassFrame::GetTimeout() {
  CString timeout;
  timeout_edit_.GetWindowText(timeout);
  return SysWideToUTF8(timeout.operator const wchar_t*());
}

CString CYassFrame::GetStatusMessage() {
  // TODO better?
  if (mApp->GetState() == CYassApp::STOPPED) {
    CString idle_message;
    if (!idle_message.LoadString(AFX_IDS_IDLEMESSAGE)) {
      idle_message = L"IDLE";
    }
    return idle_message;
  }

  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = total_rx_bytes;
    uint64_t tx_bytes = total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) /
               static_cast<double>(delta_time) * NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) /
               static_cast<double>(delta_time) * NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::stringstream ss;
  ss << mApp->GetStatus();
  ss << " tx rate: ";
  humanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << " rx rate: ";
  humanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return SysUTF8ToWide(ss.str()).c_str();
}

void CYassFrame::OnStarted() {
  LoadConfig();
  serverhost_edit_.EnableWindow(false);
  serverport_edit_.EnableWindow(false);
  password_edit_.EnableWindow(false);
  method_combo_box_.EnableWindow(false);
  localhost_edit_.EnableWindow(false);
  localport_edit_.EnableWindow(false);
  timeout_edit_.EnableWindow(false);
  autostart_button_.EnableWindow(false);
  stop_button_.EnableWindow(true);
}

void CYassFrame::OnStartFailed() {
  LoadConfig();
  serverhost_edit_.EnableWindow(true);
  serverport_edit_.EnableWindow(true);
  password_edit_.EnableWindow(true);
  method_combo_box_.EnableWindow(true);
  localhost_edit_.EnableWindow(true);
  localport_edit_.EnableWindow(true);
  timeout_edit_.EnableWindow(true);
  autostart_button_.EnableWindow(true);
  start_button_.EnableWindow(true);
  this->MessageBox(SysUTF8ToWide(mApp->GetStatus()).c_str(), _T("Start Failed"),
                   MB_ICONEXCLAMATION | MB_OK);
}

void CYassFrame::OnStopped() {
  LoadConfig();
  serverhost_edit_.EnableWindow(true);
  serverport_edit_.EnableWindow(true);
  password_edit_.EnableWindow(true);
  method_combo_box_.EnableWindow(true);
  localhost_edit_.EnableWindow(true);
  localport_edit_.EnableWindow(true);
  timeout_edit_.EnableWindow(true);
  autostart_button_.EnableWindow(true);
  start_button_.EnableWindow(true);
}

void CYassFrame::LoadConfig() {
  CString server_host(SysUTF8ToWide(absl::GetFlag(FLAGS_server_host)).c_str());
  serverhost_edit_.SetWindowText(server_host);
  CString server_port(
      std::to_wstring(absl::GetFlag(FLAGS_server_port)).c_str());
  serverport_edit_.SetWindowText(server_port);
  CString password(SysUTF8ToWide(absl::GetFlag(FLAGS_password)).c_str());
  password_edit_.SetWindowText(password);
  int32_t method = absl::GetFlag(FLAGS_cipher_method);
  for (int i = 0; i < method_combo_box_.GetCount(); ++i) {
    if (method_combo_box_.GetItemData(i) == static_cast<DWORD>(method)) {
      method_combo_box_.SetCurSel(i);
      break;
    }
  }
  CString local_host(SysUTF8ToWide(absl::GetFlag(FLAGS_local_host)).c_str());
  localhost_edit_.SetWindowText(local_host);
  CString local_port(std::to_wstring(absl::GetFlag(FLAGS_local_port)).c_str());
  localport_edit_.SetWindowText(local_port);
  CString timeout(
      std::to_wstring(absl::GetFlag(FLAGS_connect_timeout)).c_str());
  timeout_edit_.SetWindowText(timeout);
}

int CYassFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;

  // Left Panel
  CRect rect;

  if (!start_button_.Create(_T("START"), BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                            rect, this, IDC_START)) {
    LOG(WARNING) << "start button not created";
    return false;
  }

  if (!stop_button_.Create(_T("STOP"), BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                           rect, this, IDC_STOP)) {
    LOG(WARNING) << "stop button not created";
    return false;
  }

  stop_button_.EnableWindow(false);

  // Right Panel
  CString method_strings[] = {
#define XX(num, name, string) _T(string),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  DWORD method_nums[] = {
#define XX(num, name, string) static_cast<DWORD>(num),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#static-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#edit-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#combo-box-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#button-styles

  if (!serverhost_label_.Create(_T("Server Host"),
                                WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "serverhost_label not created";
    return FALSE;
  }
  if (!serverhost_edit_.Create(
          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_BORDER | ES_LEFT, rect, this,
          IDC_EDIT_SERVER_HOST)) {
    LOG(WARNING) << "serverhost_edit not created";
    return FALSE;
  }

  if (!serverport_label_.Create(_T("Server Port"),
                                WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "serverport_label not created";
    return FALSE;
  }
  if (!serverport_edit_.Create(
          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER, rect, this,
          IDC_EDIT_SERVER_PORT)) {
    LOG(WARNING) << "serverport_edit not created";
    return FALSE;
  }

  if (!password_label_.Create(_T("Password"), WS_CHILD | WS_VISIBLE | SS_LEFT,
                              rect, this)) {
    LOG(WARNING) << "password_label not created";
    return FALSE;
  }
  if (!password_edit_.Create(
          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_PASSWORD, rect, this,
          IDC_EDIT_PASSWORD)) {
    LOG(WARNING) << "password_edit not created";
    return FALSE;
  }

  if (!method_label_.Create(_T("Cipher Method"),
                            WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "method_label not created";
    return FALSE;
  }
  if (!method_combo_box_.Create(
          WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, rect, this,
          IDC_COMBOBOX_METHOD)) {
    LOG(WARNING) << "method_combo_box not created";
    return FALSE;
  }

  int method_count = sizeof(method_strings) / sizeof(method_strings[0]) - 1;
  for (int i = 0; i < method_count; ++i) {
    method_combo_box_.AddString(std::move(method_strings[i + 1]));
    method_combo_box_.SetItemData(i, method_nums[i + 1]);
  }

  method_combo_box_.SetMinVisibleItems(method_count);

  if (!localhost_label_.Create(_T("Local Host"),
                               WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "localhost_label not created";
    return FALSE;
  }
  if (!localhost_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT, rect,
                              this, IDC_EDIT_LOCAL_HOST)) {
    LOG(WARNING) << "localhost_edit not created";
    return FALSE;
  }

  if (!localport_label_.Create(_T("Local Port"),
                               WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "localport_label not created";
    return FALSE;
  }
  if (!localport_edit_.Create(
          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER, rect, this,
          IDC_EDIT_LOCAL_PORT)) {
    LOG(WARNING) << "localport_edit not created";
    return FALSE;
  }

  if (!timeout_label_.Create(_T("Timeout"), WS_CHILD | WS_VISIBLE | SS_LEFT,
                             rect, this)) {
    LOG(WARNING) << "timeout_label not created";
    return FALSE;
  }
  if (!timeout_edit_.Create(
          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER, rect, this,
          IDC_EDIT_TIMEOUT)) {
    LOG(WARNING) << "timeout_edit not created";
    return FALSE;
  }

  if (!autostart_label_.Create(_T("Auto Start"),
                               WS_CHILD | WS_VISIBLE | SS_LEFT, rect, this)) {
    LOG(WARNING) << "autostart_label not created";
    return FALSE;
  }
  if (!autostart_button_.Create(
          _T("Enable"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_LEFT, rect,
          this, IDC_AUTOSTART_CHECKBOX)) {
    LOG(WARNING) << "autostart_button not created";
    return FALSE;
  }

  autostart_button_.SetCheck(Utils::GetAutoStart() ? BST_CHECKED
                                                   : BST_UNCHECKED);
  if (!status_bar_.Create(this) ||
      !status_bar_.SetIndicators(indicators,
                                 sizeof(indicators) / sizeof(indicators[0]))) {
    LOG(WARNING) << "Failed to create status bar";
    return -1;
  }

  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, ID_APP_MSG);

  // Load the new menu.
  if (!menu_.LoadMenu(IDR_MAINFRAME)) {
    LOG(WARNING) << "Failed to load menu";
    return -1;
  }

  // Remove and destroy the old menu
  SetMenu(nullptr);
  ::DestroyMenu(m_hMenuDefault);

  // Add the new menu
  SetMenu(&menu_);

  // Assign default menu
  m_hMenuDefault = menu_.GetSafeHmenu();

  UpdateLayoutForDpi();

  LoadConfig();

  return 0;
}

void CYassFrame::UpdateLayoutForDpi() {
  UINT uDPI = Utils::GetDpiForWindowOrSystem(GetSafeHwnd());
  LOG(WARNING) << "Adjust layout to Dpi: " << uDPI;
  UpdateLayoutForDpi(uDPI);
}

void CYassFrame::UpdateLayoutForDpi(UINT uDpi) {
  // Left Panel
  CRect rect, client_rect;

  GetClientRect(&client_rect);

  rect = client_rect;
  rect.OffsetRect(COLUMN_ONE_LEFT, VERTICAL_HEIGHT);
  rect.right = rect.left + BUTTON_WIDTH;
  rect.bottom = rect.top + BUTTON_HEIGHT;

  start_button_.SetWindowPos(nullptr, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_ONE_LEFT, VERTICAL_HEIGHT * 5);
  rect.right = rect.left + BUTTON_WIDTH;
  rect.bottom = rect.top + BUTTON_HEIGHT;
  stop_button_.SetWindowPos(nullptr, rect.left, rect.top,
                            rect.right - rect.left, rect.bottom - rect.top,
                            SWP_NOZORDER | SWP_NOACTIVATE);

  // RIGHT Panel
  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  serverhost_label_.SetWindowPos(nullptr, rect.left, rect.top,
                                 rect.right - rect.left, rect.bottom - rect.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  serverhost_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                                rect.right - rect.left, rect.bottom - rect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 2);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  serverport_label_.SetWindowPos(nullptr, rect.left, rect.top,
                                 rect.right - rect.left, rect.bottom - rect.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 2);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  serverport_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                                rect.right - rect.left, rect.bottom - rect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 3);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  password_label_.SetWindowPos(nullptr, rect.left, rect.top,
                               rect.right - rect.left, rect.bottom - rect.top,
                               SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 3);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  password_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                              rect.right - rect.left, rect.bottom - rect.top,
                              SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 4);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  method_label_.SetWindowPos(nullptr, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 4);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  method_combo_box_.SetWindowPos(nullptr, rect.left, rect.top,
                                 rect.right - rect.left, rect.bottom - rect.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 5);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  localhost_label_.SetWindowPos(nullptr, rect.left, rect.top,
                                rect.right - rect.left, rect.bottom - rect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 5);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  localhost_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                               rect.right - rect.left, rect.bottom - rect.top,
                               SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 6);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  localport_label_.SetWindowPos(nullptr, rect.left, rect.top,
                                rect.right - rect.left, rect.bottom - rect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 6);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  localport_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                               rect.right - rect.left, rect.bottom - rect.top,
                               SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 7);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  timeout_label_.SetWindowPos(nullptr, rect.left, rect.top,
                              rect.right - rect.left, rect.bottom - rect.top,
                              SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 7);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  timeout_edit_.SetWindowPos(nullptr, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT, VERTICAL_HEIGHT * 8);
  rect.right = rect.left + LABEL_WIDTH;
  rect.bottom = rect.top + LABEL_HEIGHT;
  autostart_label_.SetWindowPos(nullptr, rect.left, rect.top,
                                rect.right - rect.left, rect.bottom - rect.top,
                                SWP_NOZORDER | SWP_NOACTIVATE);
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT, VERTICAL_HEIGHT * 8);
  rect.right = rect.left + EDIT_WIDTH;
  rect.bottom = rect.top + EDIT_HEIGHT;
  autostart_button_.SetWindowPos(nullptr, rect.left, rect.top,
                                 rect.right - rect.left, rect.bottom - rect.top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void CYassFrame::OnClose() {
  LOG(WARNING) << "Frame is closing ";
  // Same with CWinApp::OnAppExit
  //
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/application-information-and-management?view=msvc-170#afxgetmainwnd
  // The following line send a WM_CLOSE message to the Application's
  // main window. This will cause the Application to exit.
  AfxGetApp()->GetMainWnd()->PostMessage(WM_CLOSE);

  CFrameWnd::OnClose();
}

// https://docs.microsoft.com/en-us/cpp/mfc/updating-the-text-of-a-status-bar-pane?view=msvc-170
void CYassFrame::OnUpdateStatusBar(CCmdUI* pCmdUI) {
  UINT uiStyle = 0;
  int icxWidth = 0;

  CString csPaneString = GetStatusMessage();

  CRect rectPane;
  rectPane.SetRectEmpty();

  CDC* pDC = GetDC();

  CFont* pFont = status_bar_.GetFont();
  CFont* pOldFont = pDC->SelectObject(pFont);

  pDC->DrawText(csPaneString, rectPane, DT_CALCRECT);

  pDC->SelectObject(pOldFont);
  ReleaseDC(pDC);

  status_bar_.GetPaneInfo(0, pCmdUI->m_nID, uiStyle, icxWidth);
  status_bar_.SetPaneInfo(0, pCmdUI->m_nID, uiStyle | SBPS_STRETCH,
                          rectPane.Width());

  pCmdUI->Enable();
  pCmdUI->SetText(csPaneString);
}

LRESULT CYassFrame::OnDPIChanged(WPARAM w, LPARAM l) {
  LOG(WARNING) << "DPI changed";

  // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DPIAwarenessPerWindow/client/DpiAwarenessContext.cpp
  UINT uDpi = HIWORD(w);

  // Resize the window
  auto lprcNewScale = reinterpret_cast<RECT*>(l);

  SetWindowPos(nullptr, lprcNewScale->left, lprcNewScale->top,
               lprcNewScale->right - lprcNewScale->left,
               lprcNewScale->bottom - lprcNewScale->top,
               SWP_NOZORDER | SWP_NOACTIVATE);

  UpdateLayoutForDpi(uDpi);
  return 0;
}

void CYassFrame::OnStartButtonClicked() {
  start_button_.EnableWindow(false);
  mApp->OnStart();
}

void CYassFrame::OnStopButtonClicked() {
  stop_button_.EnableWindow(false);
  mApp->OnStop();
}

void CYassFrame::OnCheckedAutoStartButtonClicked() {
  Utils::EnableAutoStart(autostart_button_.GetCheck() & BST_CHECKED);
}
