//
// yass_frame.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "yass_frame.hpp"
#include "panels.hpp"
#include "yass.hpp"
#include <wx/stattext.h>

wxBEGIN_EVENT_TABLE(YASSFrame, wxFrame) EVT_MENU(ID_Hello, YASSFrame::OnHello)
    EVT_MENU(wxID_EXIT, YASSFrame::OnExit)
        EVT_MENU(wxID_ABOUT, YASSFrame::OnAbout) wxEND_EVENT_TABLE()

            YASSFrame::YASSFrame(const wxString &title, const wxPoint &pos,
                                 const wxSize &size)
    : wxFrame(NULL, wxID_ANY, title, pos, size) {
  wxMenu *menuFile = new wxMenu;
  menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
                   "Hell string shown in status bar for this menu item");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  wxMenu *menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);

  wxMenuBar *menuBar = new wxMenuBar;
  menuBar->Append(menuFile, "&File");
  menuBar->Append(menuHelp, "&Help");

  SetMenuBar(menuBar);

  CreateStatusBar();
  SetStatusText("Welcome to Yet-Another-Shadow-Socket!");

  wxPanel *panel = new wxPanel(this, wxID_ANY);

  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

  m_leftpanel = new LeftPanel(panel);
  m_rightpanel = new RightPanel(panel);

  hbox->Add(m_leftpanel, 1, wxEXPAND | wxALL, 5);
  hbox->Add(m_rightpanel, 1, wxEXPAND | wxALL, 5);

  panel->SetSizer(hbox);

  Centre();
}

void YASSFrame::UpdateStatus() {
  m_rightpanel->m_text->SetLabel(mApp->GetStatus());
}

void YASSFrame::OnExit(wxCommandEvent &event) { Close(true); }

void YASSFrame::OnAbout(wxCommandEvent &event) {
  wxMessageBox("This is a wxWidgets' Hello world sample", "About Hello World",
               wxOK | wxICON_INFORMATION);
}

void YASSFrame::OnHello(wxCommandEvent &event) {
  wxLogMessage("Hello world from wxWidgets!");
}
