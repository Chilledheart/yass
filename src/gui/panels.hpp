//
// panels.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
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

  void OnSetText(wxCommandEvent &event);

  wxStaticText *m_text;
};

enum { ID_START = 0x101, ID_STOP = 0x102 };

#endif // PANELS
