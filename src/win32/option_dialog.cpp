// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "win32/option_dialog.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

BEGIN_MESSAGE_MAP(COptionDialog, CDialog)
  // No message handlers
END_MESSAGE_MAP()

COptionDialog::~COptionDialog() = default;

// Windows sends the WM_INITDIALOG message to
// the dialog box during the Create, CreateIndirect,
// or DoModal calls, which occur immediately before the dialog box
// is displayed.
BOOL COptionDialog::OnInitDialog() {
  CDialog::OnInitDialog();

  // extra initialization to all fields
  connect_timeout_ = absl::GetFlag(FLAGS_connect_timeout);
  tcp_user_timeout_ = absl::GetFlag(FLAGS_tcp_user_timeout);
  tcp_so_linger_timeout_ = absl::GetFlag(FLAGS_so_linger_timeout);
  tcp_so_snd_buffer_ = absl::GetFlag(FLAGS_so_snd_buffer);
  tcp_so_rcv_buffer_ = absl::GetFlag(FLAGS_so_rcv_buffer);
  SetDlgItemInt(IDC_EDIT_CONNECT_TIMEOUT, connect_timeout_);
  SetDlgItemInt(IDC_EDIT_TCP_USER_TIMEOUT, tcp_user_timeout_);
  SetDlgItemInt(IDC_EDIT_TCP_SO_LINGER_TIMEOUT, tcp_so_linger_timeout_);
  SetDlgItemInt(IDC_EDIT_TCP_SO_SEND_BUFFER, tcp_so_snd_buffer_);
  SetDlgItemInt(IDC_EDIT_TCP_SO_RECEIVE_BUFFER, tcp_so_rcv_buffer_);

  return TRUE;
}

// https://docs.microsoft.com/en-us/cpp/mfc/reference/standard-dialog-data-exchange-routines
// https://docs.microsoft.com/en-us/cpp/mfc/dialog-data-exchange?view=msvc-170
// https://docs.microsoft.com/en-us/cpp/mfc/dialog-data-validation?view=msvc-170
// The DDV_ routine should immediately follow the DDX_ routine for that field.
void COptionDialog::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);

  DDX_Text(pDX, IDC_EDIT_CONNECT_TIMEOUT, connect_timeout_);
  DDV_MinMaxInt(pDX, connect_timeout_, 0, INT_MAX);
  DDX_Text(pDX, IDC_EDIT_TCP_USER_TIMEOUT, tcp_user_timeout_);
  DDV_MinMaxInt(pDX, tcp_user_timeout_, 0, INT_MAX);
  DDX_Text(pDX, IDC_EDIT_TCP_SO_LINGER_TIMEOUT, tcp_so_linger_timeout_);
  DDV_MinMaxInt(pDX, tcp_so_linger_timeout_, 0, INT_MAX);
  DDX_Text(pDX, IDC_EDIT_TCP_SO_SEND_BUFFER, tcp_so_snd_buffer_);
  DDV_MinMaxInt(pDX, tcp_so_snd_buffer_, 0, INT_MAX);
  DDX_Text(pDX, IDC_EDIT_TCP_SO_RECEIVE_BUFFER, tcp_so_rcv_buffer_);
  DDV_MinMaxInt(pDX, tcp_so_rcv_buffer_, 0, INT_MAX);
}
