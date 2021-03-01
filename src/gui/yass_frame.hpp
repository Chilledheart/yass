// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef YASS_FRAME
#define YASS_FRAME

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

class LeftPanel;
class RightPanel;
class YASSFrame : public wxFrame {
public:
  YASSFrame(const wxString &title, const wxPoint &pos, const wxSize &size);

  std::string GetServerHost();
  std::string GetServerPort();
  std::string GetPassword();
  std::string GetMethod();
  std::string GetLocalHost();
  std::string GetLocalPort();

  void Started();
  void Stopped();
  void StartFailed();

  void UpdateStatus();

private:
  void OnHello(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);

  void OnIdle(wxIdleEvent &event);
  void OnClose(wxCloseEvent &event);

  LeftPanel *m_leftpanel;
  RightPanel *m_rightpanel;

private:
  wxDECLARE_EVENT_TABLE();
};

enum {
  ID_Hello = 1, // NOP
};

#endif // YASS_FRAME
