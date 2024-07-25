// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "qt6/yass_window.hpp"
#include "qt6/option_dialog.hpp"
#include "qt6/yass.hpp"

#include <absl/flags/flag.h>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWindow>

#include "cli/cli_connection_stats.hpp"
#include "config/config.hpp"
#include "core/utils.hpp"
#include "feature.h"
#include "freedesktop/utils.hpp"
#include "gui_variant.h"
#include "version.h"

YASSWindow::YASSWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowIcon(QIcon::fromTheme("yass", QIcon(":/res/images/yass.png")));
  setWindowTitle(tr("YASS"));

  // Vertical Box

  QVBoxLayout* vbox = new QVBoxLayout;
  vbox->addSpacing(0);
  vbox->setContentsMargins(0, 0, 0, 0);

  // MenuBar << Vertical Box
  //
  QAction* option_action = new QAction(tr("Option"), this);
  connect(option_action, &QAction::triggered, this, &YASSWindow::OnOption);

  QAction* exit_action = new QAction(tr("Exit"), this);
  connect(exit_action, &QAction::triggered, this, [&]() { App()->quit(); });

  QMenu* menu = new QMenu(tr("File"));
  menu->addAction(option_action);
  menu->addSeparator();
  menu->addAction(exit_action);

  QAction* about_action = new QAction(tr("About"), this);
  connect(about_action, &QAction::triggered, this, &YASSWindow::OnAbout);

  QMenu* help_menu = new QMenu(tr("Help"));
  help_menu->addAction(about_action);

  QMenuBar* menubar = new QMenuBar;
  menubar->addMenu(menu);
  menubar->addMenu(help_menu);
  vbox->addWidget(menubar);

  // Grid << Vertical Box

  QGridLayout* grid = new QGridLayout;
  grid->setContentsMargins(12, 12, 12, 12);
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(6);

  // Left (Column 0) << Grid << Vertical Box

  start_button_ = new QPushButton(tr("Start"));
  stop_button_ = new QPushButton(tr("Stop"));
  connect(start_button_, &QPushButton::clicked, this, &YASSWindow::OnStartButtonClicked);
  grid->addWidget(start_button_, 2, 0);
  connect(stop_button_, &QPushButton::clicked, this, &YASSWindow::OnStopButtonClicked);
  stop_button_->setEnabled(false);
  grid->addWidget(stop_button_, 7, 0);

  // Left (Column 1) << Grid << Vertical Box

  auto server_host_label = new QLabel(tr("Server Host"));
  auto server_sni_label = new QLabel(tr("Server SNI"));
  auto server_port_label = new QLabel(tr("Server Port"));
  auto username_label = new QLabel(tr("Username"));
  auto password_label = new QLabel(tr("Password"));
  auto method_label = new QLabel(tr("Cipher/Method"));
  auto local_host_label = new QLabel(tr("Local Host"));
  auto local_port_label = new QLabel(tr("Local Port"));
  auto doh_url_label = new QLabel(tr("DNS over HTTPS URL"));
  auto dot_host_label = new QLabel(tr("DNS over TLS Host"));
  auto limit_rate_label = new QLabel(tr("Limit Rate"));
  auto timeout_label = new QLabel(tr("Timeout"));
  auto autostart_label = new QLabel(tr("Auto Start"));
  auto systemproxy_label = new QLabel(tr("System Proxy"));

  grid->addWidget(server_host_label, 0, 1);
  grid->addWidget(server_sni_label, 1, 1);
  grid->addWidget(server_port_label, 2, 1);
  grid->addWidget(username_label, 3, 1);
  grid->addWidget(password_label, 4, 1);
  grid->addWidget(method_label, 5, 1);
  grid->addWidget(local_host_label, 6, 1);
  grid->addWidget(local_port_label, 7, 1);
  grid->addWidget(doh_url_label, 8, 1);
  grid->addWidget(dot_host_label, 9, 1);
  grid->addWidget(limit_rate_label, 10, 1);
  grid->addWidget(timeout_label, 11, 1);
  grid->addWidget(autostart_label, 12, 1);
  grid->addWidget(systemproxy_label, 13, 1);

  // Right (Column 2) << Grid << Vertical Box

  server_host_ = new QLineEdit;
  server_sni_ = new QLineEdit;
  server_port_ = new QLineEdit;
  server_port_->setValidator(new QIntValidator(0, UINT16_MAX, this));
  username_ = new QLineEdit;
  password_ = new QLineEdit;
  password_->setEchoMode(QLineEdit::Password);

  static constexpr const char* const method_names[] = {
#define XX(num, name, string) string,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };

  method_ = new QComboBox;
  for (const char* method_name : method_names) {
    method_->addItem(method_name);
  }

  local_host_ = new QLineEdit;
  local_port_ = new QLineEdit;
  local_port_->setValidator(new QIntValidator(0, UINT16_MAX, this));
  doh_url_ = new QLineEdit;
  doh_url_->setPlaceholderText("https://1.1.1.1/dns-query");
  dot_host_ = new QLineEdit;
  dot_host_->setPlaceholderText("1.1.1.1");
  limit_rate_ = new QLineEdit;
  limit_rate_->setPlaceholderText("10m");
  timeout_ = new QLineEdit;
  timeout_->setValidator(new QIntValidator(0, INT32_MAX, this));

  autostart_ = new QCheckBox;
  systemproxy_ = new QCheckBox;
  autostart_->setChecked(Utils::GetAutoStart());
  systemproxy_->setChecked(Utils::GetSystemProxy());
  connect(autostart_, &QPushButton::clicked, this, &YASSWindow::OnAutoStartClicked);
  connect(systemproxy_, &QPushButton::clicked, this, &YASSWindow::OnSystemProxyClicked);

  grid->addWidget(server_host_, 0, 2);
  grid->addWidget(server_sni_, 1, 2);
  grid->addWidget(server_port_, 2, 2);
  grid->addWidget(username_, 3, 2);
  grid->addWidget(password_, 4, 2);
  grid->addWidget(method_, 5, 2);
  grid->addWidget(local_host_, 6, 2);
  grid->addWidget(local_port_, 7, 2);
  grid->addWidget(doh_url_, 8, 2);
  grid->addWidget(dot_host_, 9, 2);
  grid->addWidget(limit_rate_, 10, 2);
  grid->addWidget(timeout_, 11, 2);
  grid->addWidget(autostart_, 12, 2);
  grid->addWidget(systemproxy_, 13, 2);

  // StatusBar << Vertical Box

  vbox->addItem(grid);
  status_bar_ = new QStatusBar;
  status_bar_->showMessage(tr("READY"));
  vbox->addWidget(status_bar_);

  QWidget* wrapper = new QWidget;
  wrapper->setObjectName("mainWrapper");
  wrapper->setLayout(vbox);

  setCentralWidget(wrapper);

  LoadChanges();
}

// see https://github.com/qt/qtbase/commit/b455a863a1df61337f36f2e8b43101ca21514697
void YASSWindow::moveToCenter() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  move(screen()->geometry().center() - frameGeometry().center());
#else
  move(windowHandle()->screen()->geometry().center() - frameGeometry().center());
#endif
}

void YASSWindow::showWindow() {
  showNormal();
  show();
  raise();
  activateWindow();
}

void YASSWindow::OnStartButtonClicked() {
  start_button_->setEnabled(false);
  stop_button_->setEnabled(false);

  server_host_->setEnabled(false);
  server_sni_->setEnabled(false);
  server_port_->setEnabled(false);
  username_->setEnabled(false);
  password_->setEnabled(false);
  method_->setEnabled(false);
  local_host_->setEnabled(false);
  local_port_->setEnabled(false);
  doh_url_->setEnabled(false);
  dot_host_->setEnabled(false);
  limit_rate_->setEnabled(false);
  timeout_->setEnabled(false);

  App()->OnStart();
}

void YASSWindow::OnStopButtonClicked() {
  start_button_->setEnabled(false);
  stop_button_->setEnabled(false);

  App()->OnStop();
}

void YASSWindow::OnAutoStartClicked() {
  Utils::EnableAutoStart(autostart_->checkState() == Qt::CheckState::Checked);
}

void YASSWindow::OnSystemProxyClicked() {
  Utils::SetSystemProxy(systemproxy_->checkState() == Qt::CheckState::Checked);
}

std::string YASSWindow::GetServerHost() {
  return server_host_->text().toUtf8().data();
}

std::string YASSWindow::GetServerSNI() {
  return server_sni_->text().toUtf8().data();
}

std::string YASSWindow::GetServerPort() {
  return server_port_->text().toUtf8().data();
}

std::string YASSWindow::GetUsername() {
  return username_->text().toUtf8().data();
}

std::string YASSWindow::GetPassword() {
  return password_->text().toUtf8().data();
}

std::string YASSWindow::GetMethod() {
  return method_->currentText().toUtf8().data();
}

std::string YASSWindow::GetLocalHost() {
  return local_host_->text().toUtf8().data();
}

std::string YASSWindow::GetLocalPort() {
  return local_port_->text().toUtf8().data();
}

std::string YASSWindow::GetDoHUrl() {
  return doh_url_->text().toUtf8().data();
}

std::string YASSWindow::GetDoTHost() {
  return dot_host_->text().toUtf8().data();
}

std::string YASSWindow::GetLimitRate() {
  return limit_rate_->text().toUtf8().data();
}

std::string YASSWindow::GetTimeout() {
  return timeout_->text().toUtf8().data();
}

std::string YASSWindow::GetStatusMessage() {
  if (App()->GetState() != YASSApp::STARTED) {
    return App()->GetStatus();
  }
  uint64_t sync_time = GetMonotonicTime();
  uint64_t delta_time = sync_time - last_sync_time_;
  if (delta_time > NS_PER_SECOND) {
    uint64_t rx_bytes = net::cli::total_rx_bytes;
    uint64_t tx_bytes = net::cli::total_tx_bytes;
    rx_rate_ = static_cast<double>(rx_bytes - last_rx_bytes_) / delta_time * NS_PER_SECOND;
    tx_rate_ = static_cast<double>(tx_bytes - last_tx_bytes_) / delta_time * NS_PER_SECOND;
    last_sync_time_ = sync_time;
    last_rx_bytes_ = rx_bytes;
    last_tx_bytes_ = tx_bytes;
  }

  std::ostringstream ss;
  ss << App()->GetStatus();
  ss << tr(" tx rate: ").toUtf8().data();
  HumanReadableByteCountBin(&ss, rx_rate_);
  ss << "/s";
  ss << tr(" rx rate: ").toUtf8().data();
  HumanReadableByteCountBin(&ss, tx_rate_);
  ss << "/s";

  return ss.str();
}

void YASSWindow::Started() {
  UpdateStatusBar();

  start_button_->setEnabled(false);
  stop_button_->setEnabled(true);
}

void YASSWindow::StartFailed() {
  UpdateStatusBar();

  start_button_->setEnabled(true);
  stop_button_->setEnabled(false);

  server_host_->setEnabled(true);
  server_sni_->setEnabled(true);
  server_port_->setEnabled(true);
  username_->setEnabled(true);
  password_->setEnabled(true);
  method_->setEnabled(true);
  local_host_->setEnabled(true);
  local_port_->setEnabled(true);
  doh_url_->setEnabled(true);
  dot_host_->setEnabled(true);
  limit_rate_->setEnabled(true);
  timeout_->setEnabled(true);

  QMessageBox::warning(this, tr("Start Failed"), QString::fromStdString(App()->GetStatus()));
}

void YASSWindow::Stopped() {
  UpdateStatusBar();

  start_button_->setEnabled(true);
  stop_button_->setEnabled(false);

  server_host_->setEnabled(true);
  server_sni_->setEnabled(true);
  server_port_->setEnabled(true);
  username_->setEnabled(true);
  password_->setEnabled(true);
  method_->setEnabled(true);
  local_host_->setEnabled(true);
  local_port_->setEnabled(true);
  doh_url_->setEnabled(true);
  dot_host_->setEnabled(true);
  limit_rate_->setEnabled(true);
  timeout_->setEnabled(true);
}

void YASSWindow::LoadChanges() {
  auto server_host_str = absl::GetFlag(FLAGS_server_host);
  auto server_sni_str = absl::GetFlag(FLAGS_server_sni);
  auto server_port_str = std::to_string(absl::GetFlag(FLAGS_server_port));
  auto username_str = absl::GetFlag(FLAGS_username);
  auto password_str = absl::GetFlag(FLAGS_password);
  uint32_t cipher_method = absl::GetFlag(FLAGS_method).method;
  auto local_host_str = absl::GetFlag(FLAGS_local_host);
  auto local_port_str = std::to_string(absl::GetFlag(FLAGS_local_port));
  auto doh_url_str = absl::GetFlag(FLAGS_doh_url);
  auto dot_host_str = absl::GetFlag(FLAGS_dot_host);
  std::string limit_rate_str = absl::GetFlag(FLAGS_limit_rate);
  auto timeout_str = std::to_string(absl::GetFlag(FLAGS_connect_timeout));

  server_host_->setText(QString::fromStdString(server_host_str));
  server_sni_->setText(QString::fromStdString(server_sni_str));
  server_port_->setText(QString::fromStdString(server_port_str));
  username_->setText(QString::fromStdString(username_str));
  password_->setText(QString::fromStdString(password_str));

  static const uint32_t method_ids[] = {
#define XX(num, name, string) num,
      CIPHER_METHOD_VALID_MAP(XX)
#undef XX
  };
  unsigned int i;
  for (i = 0; i < std::size(method_ids); ++i) {
    if (cipher_method == method_ids[i])
      break;
  }

  method_->setCurrentIndex(i);

  local_host_->setText(QString::fromStdString(local_host_str));
  local_port_->setText(QString::fromStdString(local_port_str));
  doh_url_->setText(QString::fromStdString(doh_url_str));
  dot_host_->setText(QString::fromStdString(dot_host_str));
  limit_rate_->setText(QString::fromStdString(limit_rate_str));
  timeout_->setText(QString::fromStdString(timeout_str));
}

void YASSWindow::UpdateStatusBar() {
  std::string status_msg = GetStatusMessage();
  if (last_status_msg_ == status_msg) {
    return;
  }
  last_status_msg_ = status_msg;
  status_bar_->showMessage(QString::fromStdString(last_status_msg_));
}

void YASSWindow::OnOption() {
  OptionDialog dialog(this);
  dialog.exec();
}

void YASSWindow::OnAbout() {
  QString title, text;

  title = tr("About ") + QString::fromUtf8(YASS_APP_PRODUCT_NAME) + " " + QString::fromUtf8(YASS_APP_PRODUCT_VERSION);

  text += tr("Last Change: ") + QString::fromUtf8(YASS_APP_LAST_CHANGE) + "\n";
  text += tr("Enabled Feature: ") + QString::fromUtf8(YASS_APP_FEATURES) + "\n";
  text += tr("GUI Variant: ") + QString::fromUtf8(YASS_GUI_FLAVOUR) + "\n";
  text += tr("Website: ") + QString::fromUtf8(YASS_APP_WEBSITE) + "\n";
  text += tr("Copyright: ") + QString::fromUtf8(YASS_APP_COPYRIGHT) + "\n";
  text += tr("Authors: ") + QString::fromUtf8(YASS_APP_COMPANY_NAME) + "\n";
  text += tr("License: ") + tr("GNU General Public License, version 2 only") + "\n";
  QMessageBox::about(this, title, text);
}
