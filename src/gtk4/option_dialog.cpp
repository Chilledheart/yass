// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2021-2024 Chilledheart  */

#include "gtk4/option_dialog.hpp"

#include <absl/flags/flag.h>

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "gtk/utils.hpp"
#include "net/network.hpp"

extern "C" {

struct _OptionGtkDialog {
  GtkDialog parent;

  GtkWidget* tcp_keep_alive_check;
  GtkWidget* tcp_keep_alive_cnt;
  GtkWidget* tcp_keep_alive_idle_timeout;
  GtkWidget* tcp_keep_alive_interval;
  GtkWidget* enable_post_quantum_kyber;
  GtkWidget* tcp_congestion_algorithm;

  GtkWidget* okay_button;
  GtkWidget* cancel_button;
};

G_DEFINE_TYPE(OptionGtkDialog, option_dialog, GTK_TYPE_DIALOG)

static void option_dialog_init(OptionGtkDialog* win) {
  GtkBuilder* builder;
  GMenuModel* menu;

  gtk_widget_init_template(GTK_WIDGET(win));
}

static void option_dialog_dispose(GObject* object) {
  OptionGtkDialog* dialog = OPTION_DIALOG(object);
#if GTK_CHECK_VERSION(4, 8, 0)
  gtk_widget_dispose_template(GTK_WIDGET(dialog), option_dialog_get_type());
#else
  gtk_widget_unparent(dialog->tcp_keep_alive_check);
  gtk_widget_unparent(dialog->tcp_keep_alive_cnt);
  gtk_widget_unparent(dialog->tcp_keep_alive_idle_timeout);
  gtk_widget_unparent(dialog->tcp_keep_alive_interval);
  gtk_widget_unparent(dialog->enable_post_quantum_kyber);
  gtk_widget_unparent(dialog->tcp_congestion_algorithm);

  gtk_widget_unparent(dialog->okay_button);
  gtk_widget_unparent(dialog->cancel_button);
#endif

  G_OBJECT_CLASS(option_dialog_parent_class)->dispose(object);
}

static void option_dialog_class_init(OptionGtkDialogClass* cls) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(cls), "/io/github/chilledheart/yass/option_dialog.ui");

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, tcp_keep_alive_check);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, tcp_keep_alive_cnt);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, tcp_keep_alive_idle_timeout);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, tcp_keep_alive_interval);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, enable_post_quantum_kyber);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, tcp_congestion_algorithm);

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, okay_button);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(cls), OptionGtkDialog, cancel_button);

  G_OBJECT_CLASS(cls)->dispose = option_dialog_dispose;
}

OptionGtkDialog* option_dialog_new(const gchar* title, GtkWindow* parent, GtkDialogFlags flags) {
  GtkDialog* dialog = GTK_DIALOG(g_object_new(option_dialog_get_type(), nullptr, nullptr));
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  if (parent) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
  }
  if (flags & GTK_DIALOG_MODAL) {
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  }

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT) {
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  }

  return OPTIONGtk_DIALOG(dialog);
}

}  // extern "C"

OptionDialog::OptionDialog(const std::string& title, GtkWindow* parent, bool modal)
    : impl_(option_dialog_new(title.c_str(), parent, modal ? GTK_DIALOG_MODAL : GTK_DIALOG_DESTROY_WITH_PARENT)) {
  algorithms_ = net::GetTCPAvailableCongestionAlgorithms();
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkComboBoxText* tcp_congestion_algorithm = GTK_COMBO_BOX_TEXT(impl_->tcp_congestion_algorithm);
  for (const auto& algorithm : algorithms_) {
    gtk_combo_box_text_append_text(tcp_congestion_algorithm, algorithm.c_str());
  }
  G_GNUC_END_IGNORE_DEPRECATIONS

  auto okay_callback = [](GtkButton* self, gpointer pointer) {
    OptionDialog* window = (OptionDialog*)pointer;
    window->OnOkayButtonClicked();
  };

  g_signal_connect(G_OBJECT(impl_->okay_button), "clicked", G_CALLBACK(*okay_callback), this);

  auto cancel_callback = [](GtkButton* self, gpointer pointer) {
    OptionDialog* window = (OptionDialog*)pointer;
    window->OnCancelButtonClicked();
  };

  g_signal_connect(G_OBJECT(impl_->cancel_button), "clicked", G_CALLBACK(*cancel_callback), this);

  LoadChanges();

  gtk_widget_set_visible(GTK_WIDGET(impl_), true);
}

OptionDialog::~OptionDialog() {
  gtk_window_destroy(GTK_WINDOW(impl_));
}

void OptionDialog::OnOkayButtonClicked() {
  if (!OnSave()) {
    return;
  }
  config::SaveConfig();
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_response(GTK_DIALOG(impl_), GTK_RESPONSE_ACCEPT);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

void OptionDialog::OnCancelButtonClicked() {
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_dialog_response(GTK_DIALOG(impl_), GTK_RESPONSE_CANCEL);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

void OptionDialog::run() {
  gtk_window_present(GTK_WINDOW(impl_));
}

void OptionDialog::LoadChanges() {
  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->tcp_keep_alive_check), absl::GetFlag(FLAGS_tcp_keep_alive));
  auto tcp_keep_alive_cnt_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_cnt));
  auto tcp_keep_alive_idle_timeout_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout));
  auto tcp_keep_alive_interval_str = std::to_string(absl::GetFlag(FLAGS_tcp_keep_alive_interval));
  gtk_editable_set_text(GTK_EDITABLE(impl_->tcp_keep_alive_cnt), tcp_keep_alive_cnt_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->tcp_keep_alive_idle_timeout), tcp_keep_alive_idle_timeout_str.c_str());
  gtk_editable_set_text(GTK_EDITABLE(impl_->tcp_keep_alive_interval), tcp_keep_alive_interval_str.c_str());

  gtk_check_button_set_active(GTK_CHECK_BUTTON(impl_->enable_post_quantum_kyber),
                              absl::GetFlag(FLAGS_enable_post_quantum_kyber));

  auto algorithm = absl::GetFlag(FLAGS_tcp_congestion_algorithm);
  unsigned int i;
  for (i = 0; i < std::size(algorithms_); ++i) {
    if (algorithm == algorithms_[i])
      break;
  }

  // first is unset
  if (i == std::size(algorithms_)) {
    i = 0;
  }
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_combo_box_set_active(GTK_COMBO_BOX(impl_->tcp_congestion_algorithm), i);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

bool OptionDialog::OnSave() {
  auto tcp_keep_alive = gtk_check_button_get_active(GTK_CHECK_BUTTON(impl_->tcp_keep_alive_check));
  auto tcp_keep_alive_cnt = StringToIntegerU(gtk_editable_get_text(GTK_EDITABLE(impl_->tcp_keep_alive_cnt)));
  auto tcp_keep_alive_idle_timeout =
      StringToIntegerU(gtk_editable_get_text(GTK_EDITABLE(impl_->tcp_keep_alive_idle_timeout)));
  auto tcp_keep_alive_interval = StringToIntegerU(gtk_editable_get_text(GTK_EDITABLE(impl_->tcp_keep_alive_interval)));

  auto enable_post_quantum_kyber = gtk_check_button_get_active(GTK_CHECK_BUTTON(impl_->enable_post_quantum_kyber));

  if (!tcp_keep_alive_cnt.has_value() || !tcp_keep_alive_idle_timeout.has_value() ||
      !tcp_keep_alive_interval.has_value()) {
    LOG(WARNING) << "invalid options";
    return false;
  }

  absl::SetFlag(&FLAGS_tcp_keep_alive, tcp_keep_alive);
  absl::SetFlag(&FLAGS_tcp_keep_alive_cnt, tcp_keep_alive_cnt.value());
  absl::SetFlag(&FLAGS_tcp_keep_alive_idle_timeout, tcp_keep_alive_idle_timeout.value());
  absl::SetFlag(&FLAGS_tcp_keep_alive_interval, tcp_keep_alive_interval.value());

  absl::SetFlag(&FLAGS_enable_post_quantum_kyber, enable_post_quantum_kyber);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gchar* algorithm_cstr = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(impl_->tcp_congestion_algorithm));
  G_GNUC_END_IGNORE_DEPRECATIONS
  if (algorithm_cstr == nullptr || std::string_view(algorithm_cstr).empty()) {
    absl::SetFlag(&FLAGS_tcp_congestion_algorithm, std::string());
  } else {
    absl::SetFlag(&FLAGS_tcp_congestion_algorithm, algorithm_cstr);
  }
  return true;
}
