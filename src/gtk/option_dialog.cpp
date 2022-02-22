// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2022 Chilledheart  */

#include "gtk/option_dialog.hpp"

#include <absl/flags/flag.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkgrid.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "gtk/utils.hpp"

OptionDialog::OptionDialog(const Glib::ustring& title, bool modal)
    : Gtk::Dialog(title, modal),
      connecttimeout_label_("Connect Timeout"),
      tcpusertimeout_label_("TCP User Timeout"),
      lingertimeout_label_("TCP Linger Timeout"),
      sendbuffer_label_("TCP Send Buffer"),
      recvbuffer_label_("TCP Receive Buffer") {
  set_default_size(400, 240);

  static OptionDialog* window = this;

  GtkGrid *grid = GTK_GRID(gtk_grid_new());
  gtk_grid_set_row_homogeneous(grid, true);
  gtk_grid_set_column_homogeneous(grid, true);

  gtk_grid_attach(grid, GTK_WIDGET(connecttimeout_label_.gobj()), 0, 0, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(tcpusertimeout_label_.gobj()), 0, 1, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(lingertimeout_label_.gobj()), 0, 2, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(sendbuffer_label_.gobj()), 0, 3, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(recvbuffer_label_.gobj()), 0, 4, 1, 1);

  gtk_grid_attach(grid, GTK_WIDGET(connecttimeout_.gobj()), 1, 0, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(tcpusertimeout_.gobj()), 1, 1, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(lingertimeout_.gobj()), 1, 2, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(sendbuffer_.gobj()), 1, 3, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(recvbuffer_.gobj()), 1, 4, 1, 1);

  okay_button_ = GTK_BUTTON(gtk_button_new());
  gtk_button_set_label(okay_button_, "Okay");

  cancel_button_ = GTK_BUTTON(gtk_button_new());
  gtk_button_set_label(cancel_button_, "Cancel");

  auto okay_callback = []() { window->OnOkayButtonClicked(); };

  g_signal_connect(G_OBJECT(okay_button_), "clicked",
                   G_CALLBACK(okay_callback), nullptr);

  auto cancel_callback = []() { window->OnCancelButtonClicked(); };

  g_signal_connect(G_OBJECT(cancel_button_), "clicked",
                   G_CALLBACK(cancel_callback), nullptr);

  gtk_grid_attach(grid, GTK_WIDGET(okay_button_), 0, 5, 1, 1);
  gtk_grid_attach(grid, GTK_WIDGET(cancel_button_), 1, 5, 1, 1);

  gtk_container_add(get_content_area()->Gtk::Container::gobj(), GTK_WIDGET(grid));

  LoadChanges();

  get_content_area()->show_all_children();

  Utils::DisableGtkRTTI(this);

  GTK_DIALOG_GET_CLASS(gobj())->response =
      [](GtkDialog* dialog, gint p0) {
        const auto base = static_cast<GtkDialogClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(dialog))));
        if (base && base->response)
          base->response(dialog, p0);
      };

  GTK_DIALOG_GET_CLASS(gobj())->close =
      [](GtkDialog* dialog) {
        const auto base = static_cast<GtkDialogClass*>(
            g_type_class_peek_parent(G_OBJECT_GET_CLASS(G_OBJECT(dialog))));
        if (base && base->close)
          base->close(dialog);
      };
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
