// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "freedesktop/utils.hpp"

#include "config/config.hpp"
#include "core/logging.hpp"
#include "core/process_utils.hpp"
#include "core/utils.hpp"
#include "core/utils_fs.hpp"
#include "net/asio.hpp"

#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <string_view>

using namespace std::string_literals;

using namespace yass;

static constexpr const char kDefaultAutoStartName[] = "it.gui.yass";

static constexpr const std::string_view kAutoStartFileContent =
    "[Desktop Entry]\n"
    "Version=1.0\n"
    "Type=Application\n"
    "Name=yass\n"
    "Comment=Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for embedded devices and "
    "low end boxes.\n"
    "Icon=yass\n"
    "Exec=\"%s\" --background\n"
    "Terminal=false\n"
    "Categories=Network;GTK;Utility\n";

namespace {

// followed
// https://github.com/qt/qtbase/blob/7fe1198f6edb40de2299272c7523d85d7486598b/src/corelib/io/qstandardpaths_unix.cpp#L218
std::string GetConfigDir() {
  const char* config_dir_ptr = getenv("XDG_CONFIG_HOME");
  std::string config_dir;
  // spec says relative paths should be ignored
  if (config_dir_ptr == nullptr || config_dir_ptr[0] != '/') {
    config_dir = ExpandUser("~/.config");
  } else {
    config_dir = config_dir_ptr;
  }
  return config_dir;
}

std::string GetAutostartDirectory() {
  return absl::StrCat(GetConfigDir(), "/", "autostart");
}

bool IsKDE() {
  const char* desktop_ptr = getenv("XDG_SESSION_DESKTOP");
  std::string_view desktop = desktop_ptr ? std::string_view(desktop_ptr) : std::string_view();
  return desktop == "KDE" || desktop == "plasma";
}

// see https://userbase.kde.org/KDE_System_Administration/Environment_Variables
std::string GetKDESessionVersion() {
  DCHECK(IsKDE());
  const char* kde_session_ptr = getenv("KDE_SESSION_VERSION");
  return kde_session_ptr ? std::string(kde_session_ptr) : "5"s;
}
}  // namespace

bool Utils::GetAutoStart() {
  std::string autostart_desktop_path = absl::StrCat(GetAutostartDirectory(), "/", kDefaultAutoStartName, ".desktop");
  return IsFile(autostart_desktop_path);
}

void Utils::EnableAutoStart(bool on) {
  std::string autostart_desktop_path = absl::StrCat(GetAutostartDirectory(), "/", kDefaultAutoStartName, ".desktop");
  if (!on) {
    if (!RemoveFile(autostart_desktop_path)) {
      PLOG(WARNING) << "Internal error: unable to remove autostart file";
    }
    VLOG(1) << "[autostart] removed autostart entry: " << autostart_desktop_path;
  } else {
    if (!CreateDirectories(GetAutostartDirectory())) {
      PLOG(WARNING) << "Internal error: unable to create config directory";
    }

    // force update, delete first
    if (IsFile(autostart_desktop_path) && !RemoveFile(autostart_desktop_path)) {
      PLOG(WARNING) << "Internal error: unable to remove previous autostart file";
    }

    // write to target
    std::string executable_path = "yass"s;
    GetExecutablePath(&executable_path);
    std::string desktop_entry = absl::StrFormat(kAutoStartFileContent, executable_path);
    if (!WriteFileWithBuffer(autostart_desktop_path, desktop_entry)) {
      PLOG(WARNING) << "Internal error: unable to create autostart file";
    }

    VLOG(1) << "[autostart] written autostart entry: " << autostart_desktop_path;
  }

  // Update Desktop Database
  std::string _;
  std::vector<std::string> params = {"update-desktop-database"s, ExpandUser("~/.local/share/applications"s)};
  if (ExecuteProcess(params, &_, &_) != 0) {
    PLOG(WARNING) << "update-desktop-database failed";
  } else {
    VLOG(1) << "[autostart] refreshed desktop database";
  }
}

bool Utils::GetSystemProxy() {
  if (IsKDE()) {
    bool enabled;
    std::string server_addr, bypass_addr;
    if (!QuerySystemProxy_KDE(&enabled, &server_addr, &bypass_addr)) {
      return false;
    }
    return enabled && server_addr == GetLocalAddr();
  }
  bool enabled;
  std::string server_host, server_port, bypass_addr;
  if (!QuerySystemProxy(&enabled, &server_host, &server_port, &bypass_addr)) {
    return false;
  }
  auto local_host = absl::StrCat("'", absl::GetFlag(FLAGS_local_host), "'");
  auto local_port = std::to_string(absl::GetFlag(FLAGS_local_port));
  return enabled && server_host == local_host && server_port == local_port;
}

bool Utils::SetSystemProxy(bool on) {
  bool ret = true;
  if (IsKDE()) {
    bool enabled;
    std::string server_addr, bypass_addr;
    if (!QuerySystemProxy_KDE(&enabled, &server_addr, &bypass_addr)) {
      return false;
    }
    if (on) {
      server_addr = GetLocalAddr();
    }
    ret = ::SetSystemProxy_KDE(on, server_addr, bypass_addr);
  }
  bool enabled;
  std::string server_host, server_port, bypass_addr = "['localhost', '127.0.0.0/8', '::1']"s;
  ::QuerySystemProxy(&enabled, &server_host, &server_port, &bypass_addr);
  if (on) {
    server_host = absl::StrCat("'", absl::GetFlag(FLAGS_local_host), "'");
    server_port = std::to_string(absl::GetFlag(FLAGS_local_port));
  }
  return ::SetSystemProxy(on, server_host, server_port, bypass_addr) && ret;
}

std::string Utils::GetLocalAddr() {
  std::ostringstream ss;
  auto local_host = absl::GetFlag(FLAGS_local_host);
  auto local_port = absl::GetFlag(FLAGS_local_port);

  asio::error_code ec;
  auto addr = asio::ip::make_address(local_host.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address && addr.is_v6()) {
    if (addr.is_unspecified()) {
      local_host = "::1"s;
    }
    ss << "http://[" << local_host << "]:" << local_port;
  } else {
    if (host_is_ip_address && addr.is_unspecified()) {
      local_host = "127.0.0.1"s;
    }
    ss << "http://" << local_host << ":" << local_port;
  }
  return ss.str();
}

bool QuerySystemProxy(bool* enabled, std::string* server_host, std::string* server_port, std::string* bypass_addr) {
  std::string output, _;
  std::vector<std::string> params = {"gsettings"s, "get"s, "org.gnome.system.proxy"s, "mode"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *enabled = output == "'manual'"s;

  params = {"gsettings"s, "get"s, "org.gnome.system.proxy.http"s, "host"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_host = output;

  params = {"gsettings"s, "get"s, "org.gnome.system.proxy.http"s, "port"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_port = output;

  params = {"gsettings"s, "get"s, "org.gnome.system.proxy"s, "ignore-hosts"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *bypass_addr = output;
  return true;
}

bool SetSystemProxy(bool enable,
                    const std::string& server_host,
                    const std::string& server_port,
                    const std::string& bypass_addr) {
  std::string _;
  std::vector<std::string> params = {"gsettings"s, "set"s, "org.gnome.system.proxy"s, "mode"s,
                                     enable ? "'manual'"s : "'none'"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  static constexpr std::string_view kProtocol[] = {
      "org.gnome.system.proxy.http",
      "org.gnome.system.proxy.https",
      "org.gnome.system.proxy.ftp",
      "org.gnome.system.proxy.socks",
  };
  for (std::string_view protocol : kProtocol) {
    params = {"gsettings"s, "set"s, std::string(protocol), "host"s, server_host};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }

    params = {"gsettings"s, "set"s, std::string(protocol), "port"s, server_port};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }
  }

  params = {"gsettings"s, "set"s, "org.gnome.system.proxy"s, "use-same-proxy"s, "true"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"gsettings"s, "set"s, "org.gnome.system.proxy"s, "ignore-hosts"s, bypass_addr};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"gsettings"s, "set"s, "org.gnome.system.proxy"s, "mode"s, enable ? "'manual'"s : "'none'"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }
  return true;
}

bool QuerySystemProxy_KDE(bool* enabled, std::string* server_addr, std::string* bypass_addr) {
  bypass_addr->clear();

  const std::string kde_session_version = GetKDESessionVersion();
  const std::string kreadconfig = absl::StrCat("kreadconfig"s, kde_session_version);
  const std::string config_dir = GetConfigDir();
  std::string output, _;
  std::vector<std::string> params = {
      kreadconfig, "--file"s, config_dir + "/kioslaverc"s, "--group"s, "Proxy Settings"s, "--key"s, "ProxyType"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *enabled = output == "1"s;

  params = {kreadconfig, "--file"s, config_dir + "/kioslaverc"s, "--group"s, "Proxy Settings"s, "--key"s, "httpProxy"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_addr = output;

  params = {kreadconfig, "--file"s,    config_dir + "/kioslaverc"s, "--group"s, "Proxy Settings"s,
            "--key"s,    "NoProxyFor"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *bypass_addr = output;

  return true;
}

bool SetSystemProxy_KDE(bool enable, const std::string& server_addr, const std::string& bypass_addr) {
  const std::string kde_session_version = GetKDESessionVersion();
  const std::string kwriteconfig = absl::StrCat("kwriteconfig"s, kde_session_version);
  const std::string config_dir = GetConfigDir();
  std::string _;
  std::vector<std::string> params = {kwriteconfig, "--file"s,           config_dir + "/kioslaverc"s,
                                     "--group"s,   "Proxy Settings"s,   "--key"s,
                                     "ProxyType"s, enable ? "1"s : "0"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  static constexpr std::string_view kProtocol[] = {
      "httpProxy",
      "httpsProxy",
      "ftpProxy",
      "socksProxy",
  };

  for (std::string_view protocol : kProtocol) {
    params = {kwriteconfig,      "--file"s, config_dir + "/kioslaverc"s, "--group"s,
              "Proxy Settings"s, "--key"s,  std::string(protocol),       server_addr};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }
  }

  params = {kwriteconfig,  "--file"s,  config_dir + "/kioslaverc"s, "--group"s, "Proxy Settings"s, "--key"s,
            "NoProxyFor"s, bypass_addr};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"dbus-send"s, "--type=signal"s, "/KIO/Scheduler"s, "org.kde.KIO.Scheduler.reparseSlaveConfiguration"s,
            "string:''"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  return true;
}
