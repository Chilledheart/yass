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
    : wxPanel(parent, -1, wxPoint(-1, -1), wxSize(-1, -1),
              wxBORDER_THEME) {
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
  mApp->OnStart();
}

void LeftPanel::OnStop(wxCommandEvent &WXUNUSED(event)) {
  m_stop->Disable();
  mApp->OnStop();
}

RightPanel::RightPanel(wxPanel *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(315, -1),
              wxBORDER_THEME) {
  wxString methodStrings[] = {
#define XX(num, name, string) wxT(string),
      CIPHER_METHOD_MAP(XX)
#undef XX
  };
  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
  wxFlexGridSizer *fgs = new wxFlexGridSizer(7, 2, 9, 25);

  wxStaticText *serverhost = new wxStaticText(this, -1, wxT("Server Host"));
  wxStaticText *serverport = new wxStaticText(this, -1, wxT("Server Port"));
  wxStaticText *password = new wxStaticText(this, -1, wxT("Password"),
                           wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
  wxStaticText *method = new wxStaticText(this, -1, wxT("Cipher/Method"));
  wxStaticText *localhost = new wxStaticText(this, -1, wxT("Local Host"));
  wxStaticText *localport = new wxStaticText(this, -1, wxT("Local Port"));
  m_text = new wxStaticText(this, -1, wxT("Latency: "), wxPoint(40, 60));

  m_serverhost_tc = new wxTextCtrl(this, -1);
  m_serverport_tc = new wxTextCtrl(this, -1);
  m_password_tc = new wxTextCtrl(this, -1);
  m_method_tc = new wxChoice(this, -1, wxDefaultPosition, wxSize(100, -1),
                             WXSIZEOF(methodStrings), methodStrings, 0);
  m_localhost_tc = new wxTextCtrl(this, -1);
  m_localport_tc = new wxTextCtrl(this, -1);

  fgs->Add(serverhost);
  fgs->Add(m_serverhost_tc, 1, wxEXPAND);
  fgs->Add(serverport);
  fgs->Add(m_serverport_tc, 1, wxEXPAND);
  fgs->Add(password);
  fgs->Add(m_password_tc, 1, wxEXPAND);
  fgs->Add(method);
  fgs->Add(m_method_tc, 1, wxEXPAND);
  fgs->Add(localhost);
  fgs->Add(m_localhost_tc, 1, wxEXPAND);
  fgs->Add(localport);
  fgs->Add(m_localport_tc, 1, wxEXPAND);
  fgs->Add(m_text);

  fgs->AddGrowableRow(6, 1);
  fgs->AddGrowableCol(1, 1);

  hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
  this->SetSizer(hbox);
}

void RightPanel::OnSetText(wxCommandEvent &WXUNUSED(event)) {
  wxLogMessage("SetText");
}
