// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2022 Chilledheart  */

#include "gui/option_dialog.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"

OptionDialog::OptionDialog(const Glib::ustring& title, bool modal)
    : Gtk::Dialog(title, modal),
      connecttimeout_label_("Connect Timeout"),
      tcpusertimeout_label_("TCP User Timeout"),
      lingertimeout_label_("TCP Linger Timeout"),
      sendbuffer_label_("TCP Send Buffer"),
      recvbuffer_label_("TCP Receive Buffer"),
      okay_button_("Okay"),
      cancel_button_("Cancel") {
  set_default_size(400, 240);

  grid_.set_row_homogeneous(true);
  grid_.set_column_homogeneous(true);

  grid_.attach(connecttimeout_label_, 0, 0);
  grid_.attach(tcpusertimeout_label_, 0, 1);
  grid_.attach(lingertimeout_label_, 0, 2);
  grid_.attach(sendbuffer_label_, 0, 3);
  grid_.attach(recvbuffer_label_, 0, 4);

  grid_.attach(connecttimeout_, 1, 0);
  grid_.attach(tcpusertimeout_, 1, 1);
  grid_.attach(lingertimeout_, 1, 2);
  grid_.attach(sendbuffer_, 1, 3);
  grid_.attach(recvbuffer_, 1, 4);

  okay_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &OptionDialog::OnOkayButtonClicked));

  cancel_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &OptionDialog::OnCancelButtonClicked));

  grid_.attach(okay_button_, 0, 5);
  grid_.attach(cancel_button_, 1, 5);

  get_content_area()->add(grid_);

  LoadChanges();

  get_content_area()->show_all_children();
}

void OptionDialog::OnOkayButtonClicked() {
  OnSave();
  response(Gtk::RESPONSE_ACCEPT);
}

void OptionDialog::OnCancelButtonClicked() {
  response(Gtk::RESPONSE_CANCEL);
}

void OptionDialog::LoadChanges() {
  connecttimeout_.set_text(
      std::to_string(absl::GetFlag(FLAGS_connect_timeout)));
  tcpusertimeout_.set_text(
      std::to_string(absl::GetFlag(FLAGS_tcp_user_timeout)));
  lingertimeout_.set_text(
      std::to_string(absl::GetFlag(FLAGS_so_linger_timeout)));
  sendbuffer_.set_text(std::to_string(absl::GetFlag(FLAGS_so_snd_buffer)));
  recvbuffer_.set_text(std::to_string(absl::GetFlag(FLAGS_so_rcv_buffer)));
}

void OptionDialog::OnSave() {
  auto connect_timeout = StringToInteger(connecttimeout_.get_text().data());
  auto user_timeout = StringToInteger(tcpusertimeout_.get_text().data());
  auto so_linger_timeout = StringToInteger(lingertimeout_.get_text().data());
  auto so_snd_buffer = StringToInteger(sendbuffer_.get_text().data());
  auto so_rcv_buffer = StringToInteger(recvbuffer_.get_text().data());

  if (!connect_timeout.ok() || !user_timeout.ok() || !so_linger_timeout.ok() ||
      !so_snd_buffer.ok() || !so_rcv_buffer.ok()) {
    LOG(WARNING) << "invalid options";
    return;
  }

  absl::SetFlag(&FLAGS_connect_timeout, connect_timeout.value());
  absl::SetFlag(&FLAGS_tcp_user_timeout, user_timeout.value());
  absl::SetFlag(&FLAGS_so_linger_timeout, so_linger_timeout.value());
  absl::SetFlag(&FLAGS_so_snd_buffer, so_snd_buffer.value());
  absl::SetFlag(&FLAGS_so_rcv_buffer, so_rcv_buffer.value());
}
