// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021 Chilledheart  */

#include "gui/option_dialog.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

OptionDialog::OptionDialog(wxFrame* parent,
                           const wxString& title,
                           const wxPoint& pos,
                           const wxSize& size)
    : wxDialog(parent,
               wxID_ANY,
               title,
               pos,
               size,
               wxDEFAULT_DIALOG_STYLE & (~wxCLOSE_BOX) & (~wxMINIMIZE_BOX) &
                   (~wxMAXIMIZE_BOX) & (~wxRESIZE_BORDER)) {
  wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);

#if wxCHECK_VERSION(3, 1, 0)
  int vgap = parent->FromDIP(9);
  int hgap = parent->FromDIP(25);
#else
  int vgap = 9;
  int hgap = 25;
#endif
  wxFlexGridSizer* fgs = new wxFlexGridSizer(6, 2, vgap, hgap);

  wxStaticText* connecttimeout =
      new wxStaticText(this, -1, wxT("Connect Timeout"));
  wxStaticText* tcpusertimeout =
      new wxStaticText(this, -1, wxT("TCP User Timeout"));
  wxStaticText* lingertimeout =
      new wxStaticText(this, -1, wxT("TCP Linger Timeout"));
  wxStaticText* sendbuffer = new wxStaticText(this, -1, wxT("TCP Send Buffer"));
  wxStaticText* recvbuffer =
      new wxStaticText(this, -1, wxT("TCP Receive Buffer"));

  m_connecttimeout_tc = new wxTextCtrl(this, -1);
  m_tcpusertimeout_tc = new wxTextCtrl(this, -1);
  m_lingertimeout_tc = new wxTextCtrl(this, -1);
  m_sendbuffer_tc = new wxTextCtrl(this, -1);
  m_recvbuffer_tc = new wxTextCtrl(this, -1);

  fgs->Add(connecttimeout);
  fgs->Add(m_connecttimeout_tc, 1, wxEXPAND);
  fgs->Add(tcpusertimeout);
  fgs->Add(m_tcpusertimeout_tc, 1, wxEXPAND);
  fgs->Add(lingertimeout);
  fgs->Add(m_lingertimeout_tc, 1, wxEXPAND);
  fgs->Add(sendbuffer);
  fgs->Add(m_sendbuffer_tc, 1, wxEXPAND);
  fgs->Add(recvbuffer);
  fgs->Add(m_recvbuffer_tc, 1, wxEXPAND);

  fgs->AddGrowableRow(5, 1);
  fgs->AddGrowableCol(1, 1);

  hbox->Add(fgs, 1, wxALL | wxEXPAND, 12);

  vbox->Add(hbox, 1, wxALL | wxEXPAND, 1);

  wxBoxSizer* hbox_buttons = new wxBoxSizer(wxHORIZONTAL);

  m_okay = new wxButton(this, wxOK, wxT("OK"),
#if wxCHECK_VERSION(3, 1, 0)
                        parent->FromDIP(wxPoint(10, 10))
#else
                        wxPoint(10, 10)
#endif
  );
  m_cancel = new wxButton(this, wxCANCEL, wxT("Cancel"),
#if wxCHECK_VERSION(3, 1, 0)
                          parent->FromDIP(wxPoint(10, 60))
#else
                          wxPoint(10, 60)
#endif
  );

  hbox_buttons->Add(m_okay, 1, wxEXPAND, 1);
  hbox_buttons->Add(m_cancel, 1, wxEXPAND, 1);

  vbox->Add(hbox_buttons, 1, wxALL | wxEXPAND, 1);

  this->SetSizer(vbox);

  Connect(wxID_ANY, wxEVT_SHOW, wxShowEventHandler(OptionDialog::OnShow));

  Connect(wxOK, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(OptionDialog::OnOkay));

  Connect(wxCANCEL, wxEVT_COMMAND_BUTTON_CLICKED,
          wxCommandEventHandler(OptionDialog::OnCancel));

#ifdef _WIN32
  SetIcon(wxICON(IDI_ICON1));
#endif
}

void OptionDialog::OnShow(wxShowEvent& WXUNUSED(event)) {
  OnLoad();
}

void OptionDialog::OnOkay(wxCommandEvent& WXUNUSED(event)) {
  OnSave();
  wxDialog::SetAffirmativeId(wxID_OK);
  wxDialog::AcceptAndClose();
}

void OptionDialog::OnCancel(wxCommandEvent& WXUNUSED(event)) {
  wxDialog::SetAffirmativeId(wxID_CANCEL);
  wxDialog::AcceptAndClose();
}

void OptionDialog::OnLoad() {
  m_connecttimeout_tc->SetValue(std::to_string(absl::GetFlag(FLAGS_timeout)));
  m_tcpusertimeout_tc->SetValue(
      std::to_string(absl::GetFlag(FLAGS_tcp_user_timeout)));
  m_lingertimeout_tc->SetValue(
      std::to_string(absl::GetFlag(FLAGS_so_linger_timeout)));
  m_sendbuffer_tc->SetValue(std::to_string(absl::GetFlag(FLAGS_so_snd_buffer)));
  m_recvbuffer_tc->SetValue(std::to_string(absl::GetFlag(FLAGS_so_rcv_buffer)));
}

void OptionDialog::OnSave() {
  auto connect_timeout =
      StringToInteger(m_connecttimeout_tc->GetValue().operator const char*());
  auto user_timeout =
      StringToInteger(m_tcpusertimeout_tc->GetValue().operator const char*());
  auto so_linger_timeout =
      StringToInteger(m_lingertimeout_tc->GetValue().operator const char*());
  auto so_snd_buffer =
      StringToInteger(m_sendbuffer_tc->GetValue().operator const char*());
  auto so_rcv_buffer =
      StringToInteger(m_recvbuffer_tc->GetValue().operator const char*());

  if (!connect_timeout.ok() || !user_timeout.ok() || !so_linger_timeout.ok() ||
      !so_snd_buffer.ok() || !so_rcv_buffer.ok()) {
    LOG(WARNING) << "invalid options";
    return;
  }

  absl::SetFlag(&FLAGS_timeout, connect_timeout.value());
  absl::SetFlag(&FLAGS_tcp_user_timeout, user_timeout.value());
  absl::SetFlag(&FLAGS_so_linger_timeout, so_linger_timeout.value());
  absl::SetFlag(&FLAGS_so_snd_buffer, so_snd_buffer.value());
  absl::SetFlag(&FLAGS_so_rcv_buffer, so_rcv_buffer.value());
}
