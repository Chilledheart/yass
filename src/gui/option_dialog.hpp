
// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */
#ifndef OPTION_DIALOG
#define OPTION_DIALOG

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/panel.h>

class OptionDialog : public wxDialog {
 public:
  OptionDialog(wxFrame* parent,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize);

  void OnShow(wxShowEvent& event);
  void OnOkay(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);

 private:
  void OnLoad();
  void OnSave();

  wxButton* m_okay;
  wxButton* m_cancel;
  wxTextCtrl* m_connecttimeout_tc;
  wxTextCtrl* m_tcpusertimeout_tc;
  wxTextCtrl* m_lingertimeout_tc;
  wxTextCtrl* m_sendbuffer_tc;
  wxTextCtrl* m_recvbuffer_tc;
};  // OptionDialog

#endif  // OPTION_DIALOG
