//
// yass_frame.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

  void StartStats();
  void StopStats();
  void UpdateStatus();

private:
  void OnHello(wxCommandEvent &event);
  void OnExit(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);
  void OnUpdateStats(wxTimerEvent& event);

  LeftPanel *m_leftpanel;
  RightPanel *m_rightpanel;
  wxTimer *m_update_timer;

  wxDECLARE_EVENT_TABLE();
};

enum {
  ID_Hello = 1, // NOP
  ID_UPDATE_STATS = 2,
};

#endif // YASS_FRAME
