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
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << value / 1024.0
      << " " << *c;
}

static UINT BASED_CODE indicators[] = {
    ID_SEPARATOR,  // status line indicator
    ID_APP_MSG,
};

IMPLEMENT_DYNCREATE(CYassFrame, CFrameWnd)

CYassFrame::CYassFrame() {}

BEGIN_MESSAGE_MAP(CYassFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_CLOSE()
  ON_WM_SYSCOMMAND()
  // https://docs.microsoft.com/en-us/cpp/mfc/on-update-command-ui-macro?view=msvc-170
  ON_UPDATE_COMMAND_UI(ID_APP_MSG, &CYassFrame::OnUpdateStatus)
END_MESSAGE_MAP()

std::string CYassFrame::GetServerHost() {
  CString server_host;
  right_panel_.serverhost_edit_.GetWindowText(server_host);
  return SysWideToUTF8(server_host.operator const wchar_t*());
}

std::string CYassFrame::GetServerPort() {
  CString server_port;
  right_panel_.serverport_edit_.GetWindowText(server_port);
  return SysWideToUTF8(server_port.operator const wchar_t*());
}

std::string CYassFrame::GetPassword() {
  CString password;
  right_panel_.password_edit_.GetWindowText(password);
  return SysWideToUTF8(password.operator const wchar_t*());
}

enum cipher_method CYassFrame::GetMethod() {
  int method = right_panel_.method_combo_box_.GetCurSel();
  return static_cast<enum cipher_method>(method);
}

std::string CYassFrame::GetLocalHost() {
  CString local_host;
  right_panel_.localhost_edit_.GetWindowText(local_host);
  return SysWideToUTF8(local_host.operator const wchar_t*());
}

std::string CYassFrame::GetLocalPort() {
  CString local_port;
  right_panel_.localport_edit_.GetWindowText(local_port);
  return SysWideToUTF8(local_port.operator const wchar_t*());
}

std::string CYassFrame::GetTimeout() {
  CString timeout;
  right_panel_.timeout_edit_.GetWindowText(timeout);
  return SysWideToUTF8(timeout.operator const wchar_t*());
}

void CYassFrame::Started() {
  UpdateStatus();
  right_panel_.serverhost_edit_.EnableWindow(false);
  right_panel_.serverport_edit_.EnableWindow(false);
  right_panel_.password_edit_.EnableWindow(false);
  right_panel_.method_combo_box_.EnableWindow(false);
  right_panel_.localhost_edit_.EnableWindow(false);
  right_panel_.localport_edit_.EnableWindow(false);
  right_panel_.timeout_edit_.EnableWindow(false);
  left_panel_.stop_button_.SetButtonStyle(WS_CHILD | WS_VISIBLE | WS_DISABLED);
}

void CYassFrame::StartFailed() {
  UpdateStatus();
  right_panel_.serverhost_edit_.EnableWindow(true);
  right_panel_.serverport_edit_.EnableWindow(true);
  right_panel_.password_edit_.EnableWindow(true);
  right_panel_.method_combo_box_.EnableWindow(true);
  right_panel_.localhost_edit_.EnableWindow(true);
  right_panel_.localport_edit_.EnableWindow(true);
  right_panel_.timeout_edit_.EnableWindow(true);
  left_panel_.start_button_.SetButtonStyle(WS_CHILD | WS_VISIBLE);
}

void CYassFrame::Stopped() {
  UpdateStatus();
  right_panel_.serverhost_edit_.EnableWindow(true);
  right_panel_.serverport_edit_.EnableWindow(true);
  right_panel_.password_edit_.EnableWindow(true);
  right_panel_.method_combo_box_.EnableWindow(true);
  right_panel_.localhost_edit_.EnableWindow(true);
  right_panel_.localport_edit_.EnableWindow(true);
  right_panel_.timeout_edit_.EnableWindow(true);
  left_panel_.start_button_.SetButtonStyle(WS_CHILD | WS_VISIBLE);
}

void CYassFrame::UpdateStatus() {
  CString server_host(SysUTF8ToWide(absl::GetFlag(FLAGS_server_host)).c_str());
  right_panel_.serverhost_edit_.SetWindowText(server_host);
  CString server_port(
      std::to_wstring(absl::GetFlag(FLAGS_server_port)).c_str());
  right_panel_.serverport_edit_.SetWindowText(server_port);
  CString password(SysUTF8ToWide(absl::GetFlag(FLAGS_password)).c_str());
  right_panel_.password_edit_.SetWindowText(password);
  int method = absl::GetFlag(FLAGS_cipher_method);
  right_panel_.method_combo_box_.SetEditSel(method, method);
  CString local_host(SysUTF8ToWide(absl::GetFlag(FLAGS_local_host)).c_str());
  right_panel_.localhost_edit_.SetWindowText(local_host);
  CString local_port(std::to_wstring(absl::GetFlag(FLAGS_local_port)).c_str());
  right_panel_.localport_edit_.SetWindowText(local_port);
  CString timeout(
      std::to_wstring(absl::GetFlag(FLAGS_connect_timeout)).c_str());
  right_panel_.timeout_edit_.SetWindowText(timeout);

  // TODO better?
  if (mApp->GetState() == CYassApp::STOPPED) {
    CString idle_message;
    idle_message.LoadString(AFX_IDS_IDLEMESSAGE);
    status_bar_message_ = idle_message.operator const wchar_t*();
    return;
  }

  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND / 10) {
    uint64_t rx_bytes = total_rx_bytes;
    uint64_t tx_bytes = total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time *
               NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time *
               NS_PER_SECOND;
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

#if 0
  if (!status_bar_.Create(this) ||
      status_bar_.SetIndicators(indicators,
                                sizeof(indicators) / sizeof(indicators[0]))) {
    LOG(WARNING) << "Failed to create status bar";
    return -1;
  }

  EnableDocking(CBRS_ALIGN_BOTTOM);
  DockControlBar(&status_bar_);
#else
  CStatusBar* status_bar = reinterpret_cast<CStatusBar*>(GetMessageBar());
  if (status_bar->SetIndicators(indicators,
                                sizeof(indicators) / sizeof(indicators[0]))) {
    LOG(WARNING) << "Failed to create status bar";
    return -1;
  }
#endif

  DWORD dwStyle = WS_VISIBLE | WS_CHILD | CBRS_TOP;

  // save the style
  panel_.SetPaneAlignment(dwStyle & CBRS_ALL);

  // create the HWND
  CRect rect(0, 0, 0, 0);
  if (!panel_.Create(_T("Pane"), this, rect, FALSE, IDR_MAINFRAME,
                     dwStyle | WS_CLIPSIBLINGS)) {
    LOG(WARNING) << "Failed to create pane";
    return -1;
  }

  LPCTSTR lpszClass =
      AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW),
                          (HBRUSH)(COLOR_BTNFACE + 1), nullptr);
  if (!left_panel_.Create(lpszClass, dwStyle | WS_CLIPSIBLINGS, rect, &panel_,
                          AFX_IDW_DIALOGBAR + 1)) {
    LOG(WARNING) << "Failed to create left panel";
    return -1;
  }

  if (!right_panel_.Create(lpszClass, dwStyle | WS_CLIPSIBLINGS, rect, &panel_,
                           AFX_IDW_DIALOGBAR + 1)) {
    LOG(WARNING) << "Failed to create right panel";
    return -1;
  }

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
#if 0
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/cwnd-class?view=msvc-170#destroywindow
  // MFC will react by PostQuitMessage() for you,
  // hence exit the main message loop and close your app.
  //
  // If the specified window is a parent or owner window,
  // DestroyWindow automatically destroys the associated child or
  // owned windows when it destroys the parent or owner window.
  // The function first destroys child or owned windows,
  // and then it destroys the parent or owner window.
  AfxGetApp()->GetMainWnd()->DestroyWindow();
#else
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/application-information-and-management?view=msvc-170#afxgetmainwnd
  // The following line send a WM_CLOSE message to the Application's
  // main window. This will cause the Application to exit.
  AfxGetApp()->GetMainWnd()->PostMessage(WM_CLOSE);
#endif

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
  CString csPaneString(SysUTF8ToWide(status_bar_message_).c_str());

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
/* Currently this event is generated by wxMSW port. */
void CYassFrame::OnDPIChanged() {
  wxSize newSize = GetSize();
  newSize.x *= static_cast<double>(event.GetNewDPI().GetWidth()) /
               event.GetOldDPI().GetWidth();
  newSize.y *= static_cast<double>(event.GetNewDPI().GetHeight()) /
               event.GetOldDPI().GetHeight();
  SetSize(newSize);

  newSize = right_panel_.GetSize();
  newSize.x *= static_cast<double>(event.GetNewDPI().GetWidth()) /
               event.GetOldDPI().GetWidth();
  newSize.y *= static_cast<double>(event.GetNewDPI().GetHeight()) /
               event.GetOldDPI().GetHeight();
  right_panel_.SetSize(newSize);
}
#endif
