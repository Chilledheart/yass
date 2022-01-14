// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_ABOUT_DIALOG
#define YASS_WIN32_ABOUT_DIALOG

#include <afxext.h>  // MFC extensions (including VB)
#include <afxtempl.h>
#include <afxwin.h>  // MFC core and standard components

#include "win32/resource.hpp"

class CAboutDlg : public CDialog {
 public:
  explicit CAboutDlg() : CDialog(IDD_ABOUTBOX, nullptr) {}

 protected:
  void DoDataExchange(CDataExchange* pDX) override;

 protected:
  DECLARE_MESSAGE_MAP()
};

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
  // No message handlers
END_MESSAGE_MAP()

#endif  // YASS_WIN32_ABOUT_DIALOG
