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
#include <base/posix/eintr_wrapper.h>
#include <fcntl.h>
#include <unistd.h>
#include <string_view>

using namespace std::string_literals;

using namespace yass;

static constexpr const char kDefaultAutoStartName[] = "io.github.chilledheart.yass";

#ifdef FLATPAK_BUILD
static constexpr const std::string_view kAutoStartFileContent =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=yass\n"
    "Comment=Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for embedded devices and "
    "low end boxes.\n"
    "Icon=io.github.chilledheart.yass\n"
    "Exec=/usr/bin/flatpak run --command=yass io.github.chilledheart.yass --background\n"
    "Terminal=false\n"
    "Categories=Network;GTK;Utility\n"
    "X-Flatpak=io.github.chilledheart.yass\n";
#else
static constexpr const std::string_view kAutoStartFileContent =
    "[Desktop Entry]\n"
    "Version=1.0\n"
    "Type=Application\n"
    "Name=yass\n"
    "Comment=Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for embedded devices and "
    "low end boxes.\n"
    "Icon=io.github.chilledheart.yass\n"
    "Exec=\"%s\" --background\n"
    "Terminal=false\n"
    "Categories=Network;GTK;Utility\n";
#endif

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

// https://github.com/qt/qtbase/blob/7fe1198f6edb40de2299272c7523d85d7486598b/src/corelib/io/qstandardpaths_unix.cpp#L201
std::string GetDataDir() {
  const char* data_dir_ptr = getenv("XDG_DATA_HOME");
  std::string data_dir;
  // spec says relative paths should be ignored
  if (data_dir_ptr == nullptr || data_dir_ptr[0] != '/') {
    data_dir = ExpandUser("~/.local/share");
  } else {
    data_dir = data_dir_ptr;
  }
  return data_dir;
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
#ifdef FLATPAK_BUILD
  return "5"s;
#else
  const char* kde_session_ptr = getenv("KDE_SESSION_VERSION");
  return kde_session_ptr ? std::string(kde_session_ptr) : "5"s;
#endif
}

#ifdef FLATPAK_BUILD
bool copyFileInplace(const std::string& src_file, const std::string& dst_file) {
  bool ret = false;
  int src_fd = open(src_file.c_str(), O_RDONLY);
  int dst_fd = open(dst_file.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0644);
  if (src_fd == -1) {
    PLOG(WARNING) << "copyFile: source file " << src_file << " does not exist";
    goto done;
  }
  if (dst_fd == -1) {
    PLOG(WARNING) << "copyFile: dest file " << dst_file << " cannot be created";
    goto done;
  }
  while (true) {
    char buf[4096];
    ssize_t r = HANDLE_EINTR(read(src_fd, buf, sizeof(buf)));
    if (r < 0) {
      PLOG(WARNING) << "copyFile: read error, src file: " << src_file;
      break;
    }
    if (r == 0) {
      VLOG(2) << "copyFile: read eof from file: " << src_file;
      ret = true;
      break;
    }
    VLOG(2) << "copyFile: read chunk from file: " << src_file << " with " << r << " bytes";
    ssize_t w = HANDLE_EINTR(write(dst_fd, buf, r));
    if (w < 0) {
      PLOG(WARNING) << "copyFile: write error, dest file: " << dst_file;
      break;
    }
    if (w < r) {
      PLOG(WARNING) << "copyFile: paritial write, dest file: " << dst_file << " written " << w << " in " << r
                    << " bytes ";
      break;
    }
    VLOG(2) << "copyFile: written chunk to file: " << dst_file << " with " << w << " bytes";
  }

done:
  if (src_fd != -1) {
    close(src_fd);
  }
  if (dst_fd != -1) {
    close(dst_fd);
  }
  if (ret) {
    VLOG(1) << "copyFile: copied successfully from file: " << src_file << " to file: " << dst_file;
  }
  return ret;
}
#endif
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
#ifdef FLATPAK_BUILD
    auto desktop_entry = std::string(kAutoStartFileContent);
#else
    std::string executable_path = "yass"s;
    GetExecutablePath(&executable_path);
    std::string desktop_entry = absl::StrFormat(kAutoStartFileContent, executable_path);
#endif
    if (!WriteFileWithBuffer(autostart_desktop_path, desktop_entry)) {
      PLOG(WARNING) << "Internal error: unable to create autostart file";
    }

    VLOG(1) << "[autostart] written autostart entry: " << autostart_desktop_path;
  }

  // Update Desktop Database
  std::string _;
  std::vector<std::string> params = {"update-desktop-database"s, absl::StrCat(GetDataDir(), "/applications"s)};
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
    return enabled && (server_addr == GetLocalAddrKDE() || server_addr == GetLocalAddr());
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

std::string Utils::GetLocalAddrKDE() {
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
    ss << "http://[" << local_host << "] " << local_port;
  } else {
    if (host_is_ip_address && addr.is_unspecified()) {
      local_host = "127.0.0.1"s;
    }
    ss << "http://" << local_host << " " << local_port;
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
  const std::string config_file = absl::StrCat(GetConfigDir(), "/kioslaverc");
  std::string output, _;
  std::vector<std::string> params = {kreadconfig,       "--file"s, config_file, "--group"s,
                                     "Proxy Settings"s, "--key"s,  "ProxyType"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *enabled = output == "1"s;

  params = {kreadconfig, "--file"s, config_file, "--group"s, "Proxy Settings"s, "--key"s, "httpProxy"s};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (!output.empty() && output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_addr = output;

  params = {kreadconfig, "--file"s, config_file, "--group"s, "Proxy Settings"s, "--key"s, "NoProxyFor"s};
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

  const std::string origin_config_file = absl::StrCat(GetConfigDir(), "/kioslaverc");
#ifdef FLATPAK_BUILD
  const std::string config_file = absl::StrCat(ExpandUser("~/.yass"), "/kioslaverc");
#else
  const std::string config_file = origin_config_file;
#endif
  std::string _;

#ifdef FLATPAK_BUILD
  if (!copyFileInplace(origin_config_file, config_file)) {
    return false;
  }
#endif

  std::vector<std::string> params = {kwriteconfig,      "--file"s, config_file,  "--group"s,
                                     "Proxy Settings"s, "--key"s,  "ProxyType"s, enable ? "1"s : "0"s};
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
    params = {kwriteconfig,      "--file"s, config_file,           "--group"s,
              "Proxy Settings"s, "--key"s,  std::string(protocol), server_addr};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }
  }

  params = {kwriteconfig, "--file"s, config_file, "--group"s, "Proxy Settings"s, "--key"s, "NoProxyFor"s, bypass_addr};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

#ifdef FLATPAK_BUILD
  // FIXME for KDE, system proxy might not work as expected
  // if config file ~/.config/kioslaverc is not created before app starts

  if (!copyFileInplace(config_file, origin_config_file)) {
    return false;
  }

  if (unlink((config_file).c_str()) < 0) {
    PLOG(INFO) << "Failed to remove temporary config file: " << config_file;
  }
#endif

  params = {"dbus-send"s, "--type=signal"s, "/KIO/Scheduler"s, "org.kde.KIO.Scheduler.reparseSlaveConfiguration"s,
            "string:''"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  return true;
}
