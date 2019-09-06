//
// panels.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "panels.hpp"
#include "yass.hpp"
#include <wx/stattext.h>

LeftPanel::LeftPanel(wxPanel *parent)
    : wxPanel(parent, -1, wxPoint(-1, -1), wxSize(-1, -1), wxBORDER_SUNKEN) {
  m_parent = parent;
  m_start = new wxButton(this, ID_START, wxT("START"), wxPoint(10, 10));
  m_stop = new wxButton(this, ID_STOP, wxT("STOP"), wxPoint(10, 60));

  Connect(ID_START, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(LeftPanel::OnStart));

  Connect(ID_STOP, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(LeftPanel::OnStop));

  m_stop->Disable();
}

void LeftPanel::OnStart(wxCommandEvent &WXUNUSED(event)) {
  m_start->Disable();
  m_stop->Enable();
  wxLogMessage("Start");
  mApp->OnStart();
}

void LeftPanel::OnStop(wxCommandEvent &WXUNUSED(event)) {
  m_stop->Disable();
  m_start->Enable();
  wxLogMessage("Stop");
  mApp->OnStop();
}

RightPanel::RightPanel(wxPanel *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(250, 150),
              wxBORDER_SUNKEN) {
  m_text = new wxStaticText(this, -1, wxT("READY"), wxPoint(40, 60));
}

void RightPanel::OnSetText(wxCommandEvent &WXUNUSED(event)) {
  wxLogMessage("SetText");
}
