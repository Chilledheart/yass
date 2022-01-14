// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_WIN32_PANELS
#define YASS_WIN32_PANELS

#include <afxDockablePane.h>  // CDockablePane
#include <afxext.h>           // MFC extensions (including VB)
#include <afxtempl.h>
#include <afxwin.h>  // MFC core and standard components

class LeftPanel : public CDockablePane {
  DECLARE_DYNCREATE(LeftPanel)

 public:
  LeftPanel();
  ~LeftPanel();

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

  void OnStart();
  void OnStop();

  CButton start_button_;
  CButton stop_button_;

 protected:
  DECLARE_MESSAGE_MAP();

 private:
  CPane* parent_;
};

class RightPanel : public CDockablePane {
  DECLARE_DYNCREATE(RightPanel)

 public:
  RightPanel();
  ~RightPanel();

  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);

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
  DECLARE_MESSAGE_MAP();

 private:
  CPane* parent_;
};

#endif  // YASS_WIN32_PANELS
