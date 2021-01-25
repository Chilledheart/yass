// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */
#ifndef PANELS
#define PANELS

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/panel.h>

class LeftPanel : public wxPanel {
public:
  LeftPanel(wxPanel *parent);

  void OnStart(wxCommandEvent &event);
  void OnStop(wxCommandEvent &event);

  wxButton *m_start;
  wxButton *m_stop;
  wxPanel *m_parent;
};

class RightPanel : public wxPanel {
public:
  RightPanel(wxPanel *parent);

  void OnCheckedAutoStart(wxCommandEvent &event);

  wxTextCtrl *m_serverhost_tc;
  wxTextCtrl *m_serverport_tc;
  wxTextCtrl *m_password_tc;
  wxChoice *m_method_tc;
  wxTextCtrl *m_localhost_tc;
  wxTextCtrl *m_localport_tc;
  wxCheckBox *m_autostart_cb;
};

enum { ID_START = 0x101, ID_STOP = 0x102, ID_AUTOSTART = 0x103 };

#endif // PANELS
