// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "win32/yass_frame.hpp"

#include <absl/flags/flag.h>
#include <iomanip>
#include <sstream>

#include "cli/socks5_connection_stats.hpp"
#include "core/utils.hpp"
#include "win32/about_dialog.hpp"
#include "win32/option_dialog.hpp"
#include "win32/panels.hpp"
#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass.hpp"

#define DPI_SCALE_FACTOR 2

#define COLUMN_ONE_LEFT    20
#define COLUMN_TWO_LEFT    120
#define COLUMN_THREE_LEFT  240

#define VERTICAL_HEIGHT   20

#define BUTTON_WIDTH     60
#define BUTTON_HEIGHT    20

#define LABEL_WIDTH      60
#define LABEL_HEIGHT     20
#define EDIT_WIDTH       120
#define EDIT_HEIGHT      15

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
    ID_SEPARATOR,  // status line indicator
    ID_APP_MSG,
};

IMPLEMENT_DYNCREATE(CYassFrame, CFrameWnd)

CYassFrame::CYassFrame() = default;
CYassFrame::~CYassFrame() = default;

BEGIN_MESSAGE_MAP(CYassFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_CLOSE()
  ON_WM_SYSCOMMAND()
  // https://docs.microsoft.com/en-us/cpp/mfc/on-update-command-ui-macro?view=msvc-170
  ON_UPDATE_COMMAND_UI(ID_APP_MSG, &CYassFrame::OnUpdateStatus)
#if 0
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows?redirectedfrom=MSDN
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-reference
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-getdpiscaledsize
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-beforeparent
  // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged-afterparent
  ON_MESSAGE(WM_DPICHANGED, &CYassFrame::OnDPIChanged).
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

enum cipher_method CYassFrame::GetMethod() {
  int method = static_cast<int>(method_combo_box_.GetItemData(
      method_combo_box_.GetCurSel()));
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

void CYassFrame::OnStarted() {
  UpdateStatus();
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
  UpdateStatus();
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

void CYassFrame::OnStopped() {
  UpdateStatus();
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

void CYassFrame::UpdateStatus() {
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

  // TODO better?
  if (mApp->GetState() == CYassApp::STOPPED) {
    CString idle_message;
    if (!idle_message.LoadString(AFX_IDS_IDLEMESSAGE)) {
      idle_message = L"IDLE";
    }
    status_bar_message_ = idle_message.operator const wchar_t*();
    return;
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

  status_bar_message_ = SysUTF8ToWide(ss.str());
}

int CYassFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;

  static_assert(
      (IDM_OPTIONBOX & 0xFFF0) == IDM_OPTIONBOX && IDM_OPTIONBOX < 0xF000,
      "IDM_OPTIONBOX must be in the system command range.");
  static_assert(
      (IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX && IDM_ABOUTBOX < 0xF000,
      "IDM_ABOUTBOX must be in the system command range.");

  // Left Panel
  CRect rect, client_rect;

  GetClientRect(&client_rect);

  rect = client_rect;
  rect.OffsetRect(COLUMN_ONE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * DPI_SCALE_FACTOR);
  rect.right = rect.left + BUTTON_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + BUTTON_HEIGHT * DPI_SCALE_FACTOR;

  if (!start_button_.Create(_T("START"), BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,
                            rect, this, IDC_START)) {
    LOG(WARNING) << "start button not created";
    return false;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_ONE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 5 * DPI_SCALE_FACTOR);
  rect.right = rect.left + BUTTON_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + BUTTON_HEIGHT * DPI_SCALE_FACTOR;

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

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!serverhost_label_.Create(_T("Server Host"), WS_CHILD | WS_VISIBLE |
                                SS_LEFT, rect, this)) {
    LOG(WARNING) << "serverhost_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!serverhost_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_BORDER |
                               ES_LEFT, rect, this, IDC_EDIT_SERVER_HOST)) {
    LOG(WARNING) << "serverhost_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 2 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!serverport_label_.Create(_T("Server Port"), WS_CHILD | WS_VISIBLE |
                                SS_LEFT, rect, this)) {
    LOG(WARNING) << "serverport_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 2 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!serverport_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                               ES_NUMBER, rect, this, IDC_EDIT_SERVER_PORT)) {
    LOG(WARNING) << "serverport_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 3 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!password_label_.Create(_T("Password"), WS_CHILD | WS_VISIBLE | SS_LEFT,
                              rect, this)) {
    LOG(WARNING) << "password_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 3 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!password_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                             ES_PASSWORD, rect, this, IDC_EDIT_PASSWORD)) {
    LOG(WARNING) << "password_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 4 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!method_label_.Create(_T("Cipher Method"), WS_CHILD | WS_VISIBLE |
                            SS_LEFT, rect, this)) {
    LOG(WARNING) << "method_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 4 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
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

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 5 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!localhost_label_.Create(_T("Local Host"), WS_CHILD | WS_VISIBLE |
                               SS_LEFT, rect, this)) {
    LOG(WARNING) << "localhost_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 5 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!localhost_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT, rect,
                              this, IDC_EDIT_LOCAL_HOST)) {
    LOG(WARNING) << "localhost_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 6 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!localport_label_.Create(_T("Local Port"), WS_CHILD | WS_VISIBLE |
                               SS_LEFT, rect, this)) {
    LOG(WARNING) << "localport_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 6 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!localport_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                              ES_NUMBER, rect, this, IDC_EDIT_LOCAL_PORT)) {
    LOG(WARNING) << "localport_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 7 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!timeout_label_.Create(_T("Timeout"), WS_CHILD | WS_VISIBLE | SS_LEFT,
                             rect, this)) {
    LOG(WARNING) << "timeout_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 7 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!timeout_edit_.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
                            ES_NUMBER, rect, this, IDC_EDIT_TIMEOUT)) {
    LOG(WARNING) << "timeout_edit not created";
    return FALSE;
  }

  rect = client_rect;
  rect.OffsetRect(COLUMN_TWO_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 8 * DPI_SCALE_FACTOR);
  rect.right = rect.left + LABEL_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + LABEL_HEIGHT * DPI_SCALE_FACTOR;
  if (!autostart_label_.Create(_T("Auto Start"), WS_CHILD | WS_VISIBLE |
                               SS_LEFT, rect, this)) {
    LOG(WARNING) << "autostart_label not created";
    return FALSE;
  }
  rect = client_rect;
  rect.OffsetRect(COLUMN_THREE_LEFT * DPI_SCALE_FACTOR,
                  VERTICAL_HEIGHT * 8 * DPI_SCALE_FACTOR);
  rect.right = rect.left + EDIT_WIDTH * DPI_SCALE_FACTOR;
  rect.bottom = rect.top + EDIT_HEIGHT * DPI_SCALE_FACTOR;
  if (!autostart_button_.Create(_T("Enable"), WS_CHILD | WS_VISIBLE |
                                BS_AUTOCHECKBOX | BS_LEFT, rect, this,
                                IDC_AUTOSTART_CHECKBOX)) {
    LOG(WARNING) << "autostart_button not created";
    return FALSE;
  }

  LOG(WARNING) << "Auto start: " << std::boolalpha << Utils::GetAutoStart();
  autostart_button_.SetCheck(Utils::GetAutoStart() ? BST_CHECKED
                                                   : BST_UNCHECKED);
  if (!status_bar_.Create(this) ||
      !status_bar_.SetIndicators(indicators,
                                 sizeof(indicators) / sizeof(indicators[0]))) {
    LOG(WARNING) << "Failed to create status bar";
    return -1;
  }

  RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST,
                 ID_APP_MSG);

  if (!menu_bar_.Create(this, AFX_DEFAULT_TOOLBAR_STYLE)) {
    LOG(WARNING) << "Failed to create menu bar";
    return -1;
  }

  menu_bar_.SetPaneStyle(
      (menu_bar_.GetPaneStyle() &
       ~(CBRS_GRIPPER | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM |
         CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT)) |
      CBRS_SIZE_DYNAMIC);
  menu_bar_.SetBorders();

  return 0;
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

void CYassFrame::OnSysCommand(UINT nID, LPARAM lParam) {
  if ((nID & 0xFFF0) == IDM_OPTIONBOX) {
    COptionDialog dialog(this);
    if (dialog.DoModal() == IDOK)
      mApp->SaveConfigToDisk();
  } else if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
  } else {
    CFrameWnd::OnSysCommand(nID, lParam);
  }
}

// https://docs.microsoft.com/en-us/cpp/mfc/updating-the-text-of-a-status-bar-pane?view=msvc-170
void CYassFrame::OnUpdateStatus(CCmdUI* pCmdUI) {
  UINT uiStyle = 0;
  int icxWidth = 0;

  UpdateStatus();
  CString csPaneString(status_bar_message_.c_str());

  CRect rectPane;
  rectPane.SetRectEmpty();

  CDC* pDC = GetDC();

  CFont* pFont = status_bar_.GetFont();
  CFont* pOldFont = pDC->SelectObject(pFont);

  pDC->DrawText(csPaneString, rectPane, DT_CALCRECT);

  pDC->SelectObject(pOldFont);
  ReleaseDC(pDC);

  status_bar_.GetPaneInfo(1, pCmdUI->m_nID, uiStyle, icxWidth);
  status_bar_.SetPaneInfo(1, pCmdUI->m_nID, uiStyle, rectPane.Width());

  pCmdUI->Enable();
  pCmdUI->SetText(csPaneString);
}

#if 0
void CYassFrame::OnDPIChanged(WPARAM w, LPARAM l) {
  CSize newSize = GetSize();
  newSize.x *= static_cast<double>(event.GetNewDPI().GetWidth()) /
               event.GetOldDPI().GetWidth();
  newSize.y *= static_cast<double>(event.GetNewDPI().GetHeight()) /
               event.GetOldDPI().GetHeight();
  SetSize(newSize);
}
#endif

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

