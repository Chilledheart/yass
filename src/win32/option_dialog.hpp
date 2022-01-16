// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_OPTION_DIALOG
#define YASS_WIN32_OPTION_DIALOG

#include <afxext.h>  // MFC extensions (including VB)
#include <afxtempl.h>
#include <afxwin.h>  // MFC core and standard components

#include "win32/resource.hpp"

class COptionDialog : public CDialog {
 public:
  explicit COptionDialog(CWnd* pParentWnd = NULL)
      : CDialog(IDD_OPTIONBOX, pParentWnd) {}

  ~COptionDialog() override;

 protected:
  BOOL OnInitDialog() override;
  void DoDataExchange(CDataExchange* pDX) override;

 public:
  int connect_timeout_;
  int tcp_user_timeout_;
  int tcp_so_linger_timeout_;
  int tcp_so_snd_buffer_;
  int tcp_so_rcv_buffer_;

  DECLARE_MESSAGE_MAP()
};  // COptionDialog

#endif  // YASS_WIN32_OPTION_DIALOG
