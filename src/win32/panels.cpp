// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "win32/panels.hpp"

#include "win32/resource.hpp"
#include "win32/utils.hpp"
#include "win32/yass.hpp"

BEGIN_MESSAGE_MAP(LeftPanel, CPane)
  ON_WM_CREATE()
END_MESSAGE_MAP()

int LeftPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CPane::OnCreate(lpCreateStruct) == -1)
    return -1;

  CRect rect{0, 0, 10, 10};
  start_button_.Create(_T("START"), BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE, rect,
                       this, IDC_START);

  rect = CRect{0, 0, 10, 60};
  stop_button_.Create(_T("STOP"), BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE, rect,
                      this, IDC_STOP);

  stop_button_.EnableWindow(false);

  return TRUE;
}

void LeftPanel::OnStart() {
  start_button_.EnableWindow(false);
  mApp->OnStart();
}

void LeftPanel::OnStop() {
  stop_button_.EnableWindow(false);
  mApp->OnStop();
}

BEGIN_MESSAGE_MAP(RightPanel, CPane)
  ON_WM_CREATE()
END_MESSAGE_MAP()

int RightPanel::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  if (CPane::OnCreate(lpCreateStruct) == -1)
    return -1;

  CString method_strings[] = {
#define XX(num, name, string) _T(string),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };

  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#static-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#edit-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#combo-box-styles
  // https://docs.microsoft.com/en-us/cpp/mfc/reference/styles-used-by-mfc?view=msvc-170#button-styles
  CRect rect;
  rect = CRect{0, 0, 9, 29};
  serverhost_label_.Create(_T("Server Host"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0, 9 + 100, 29};
  serverhost_edit_.Create(ES_LEFT, rect, this, IDR_MAINFRAME);

  rect = CRect{0, 0 + 10, 9, 29 + 10};
  serverport_label_.Create(_T("Server Port"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 10, 9 + 100, 29 + 10};
  serverhost_edit_.Create(ES_LEFT | ES_NUMBER, rect, this, IDR_MAINFRAME);

  rect = CRect{0, 0 + 20, 9, 29 + 20};
  password_label_.Create(_T("Password"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 20, 9 + 100, 29 + 20};
  password_edit_.Create(ES_LEFT | ES_PASSWORD, rect, this, IDR_MAINFRAME);

  rect = CRect{0, 0 + 30, 9, 29 + 30};
  method_label_.Create(_T("Cipher Method"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 30, 9 + 100, 29 + 30};
  method_combo_box_.Create(
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, rect, this,
      IDR_MAINFRAME);
  for (auto& method_string : method_strings)
    method_combo_box_.AddString(std::move(method_string));

  rect = CRect{0, 0 + 40, 9, 29 + 40};
  localhost_label_.Create(_T("Local Host"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 40, 9 + 100, 29 + 40};
  localhost_edit_.Create(ES_LEFT, rect, this, IDR_MAINFRAME);

  rect = CRect{0, 0 + 50, 9, 29 + 50};
  localport_label_.Create(_T("Local Port"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 50, 9 + 100, 29 + 50};
  localport_edit_.Create(ES_LEFT | ES_NUMBER, rect, this, IDR_MAINFRAME);

  rect = CRect{0, 0 + 60, 9, 29 + 60};
  autostart_label_.Create(_T("Auto Start"), SS_LEFT, rect, this);
  rect = CRect{0 + 100, 0 + 60, 9 + 100, 29 + 60};
  autostart_button_.Create(_T("Enable"), BS_CHECKBOX | BS_LEFT, rect, this,
                           IDR_MAINFRAME);

  autostart_button_.SetState(Utils::GetAutoStart() ? BST_CHECKED
                                                   : BST_UNCHECKED);

  return TRUE;
}

void RightPanel::OnCheckedAutoStart() {
  Utils::EnableAutoStart(autostart_button_.GetState() & BST_CHECKED);
}
