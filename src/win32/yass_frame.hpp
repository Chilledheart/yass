// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef YASS_WIN32_FRAME
#define YASS_WIN32_FRAME

#include <afxext.h>           // MFC extensions (including VB)
#include <afxmenubar.h>       // CMFCMenuBar
#include <afxtempl.h>
#include <afxwin.h>  // MFC core and standard components

#include "crypto/crypter_export.hpp"
#include "win32/panels.hpp"

#include <string>

class CYassFrame : public CFrameWnd {
  DECLARE_DYNCREATE(CYassFrame);

 protected:
  CYassFrame();

 public:
  ~CYassFrame() override;

 public:
  std::string GetServerHost();
  std::string GetServerPort();
  std::string GetPassword();
  enum cipher_method GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();
  std::string GetTimeout();

  void Started();
  void Stopped();
  void StartFailed();

  void UpdateStatus();

  // Left Panel
 public:
  void OnStart();
  void OnStop();

 protected:
  CButton start_button_;
  CButton stop_button_;

  // Right Panel
 protected:
  void OnCheckedAutoStart();

  CStatic serverhost_label_;
  CStatic serverport_label_;
  CStatic password_label_;
  CStatic method_label_;
  CStatic localhost_label_;
  CStatic localport_label_;
  CStatic timeout_label_;
  CStatic autostart_label_;

  CEdit serverhost_edit_;
  CEdit serverport_edit_;
  CEdit password_edit_;
  CComboBox method_combo_box_;
  CEdit localhost_edit_;
  CEdit localport_edit_;
  CEdit timeout_edit_;
  CButton autostart_button_;

 protected:
  CStatusBar status_bar_;

 protected:
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  // In the framework, when the user closes the frame window,
  // the window's default OnClose handler calls DestroyWindow.
  // When the main window closes, the application closes.
  afx_msg void OnClose();
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

  afx_msg void OnUpdateStatus(CCmdUI* pCmdUI);

 protected:
#if 0
  afx_msg void OnDPIChanged(WPARAM w, LPARAM l);
#endif

 private:
  std::wstring status_bar_message_;

 private:
  friend class CYassApp;

  CMFCMenuBar menu_bar_;

 protected:
  DECLARE_MESSAGE_MAP();

 private:
  uint64_t last_sync_time_ = 0;
  uint64_t last_rx_bytes_ = 0;
  uint64_t last_tx_bytes_ = 0;
  uint64_t rx_rate_ = 0;
  uint64_t tx_rate_ = 0;
};

#endif  // YASS_WIN32_FRAME
