// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "gtk/utils.hpp"

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
#include <limits.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string_view>

// Available in 2.58
#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc)(void (*)(void))(f))
#endif

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
  auto local_host = "'" + absl::GetFlag(FLAGS_local_host) + "'";
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
    server_host = "'" + absl::GetFlag(FLAGS_local_host) + "'";
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
      local_host = "::1";
    }
    ss << "http://[" << local_host << "]:" << local_port;
  } else {
    if (host_is_ip_address && addr.is_unspecified()) {
      local_host = "127.0.0.1";
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
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *enabled = output == "'manual'";

  params = {"gsettings", "get", "org.gnome.system.proxy.http", "host"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_host = output;

  params = {"gsettings", "get", "org.gnome.system.proxy.http", "port"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_port = output;

  params = {"gsettings", "get", "org.gnome.system.proxy", "ignore-hosts"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
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

  static const char* kProtocol[] = {
      "org.gnome.system.proxy.http",
      "org.gnome.system.proxy.https",
      "org.gnome.system.proxy.ftp",
      "org.gnome.system.proxy.socks",
  };
  for (const char* protocol : kProtocol) {
    params = {"gsettings", "set", protocol, "host", server_host};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }

    params = {"gsettings", "set", protocol, "port", server_port};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }
  }

  params = {"gsettings", "set", "org.gnome.system.proxy", "use-same-proxy", "true"};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"gsettings", "set", "org.gnome.system.proxy", "ignore-hosts", bypass_addr};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"gsettings", "set", "org.gnome.system.proxy", "mode", enable ? "'manual'" : "'none'"};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }
  return true;
}

bool QuerySystemProxy_KDE(bool* enabled, std::string* server_addr, std::string* bypass_addr) {
  bypass_addr->clear();

  std::string config_dir = GetConfigDir();
  std::string output, _;
  std::vector<std::string> params = {
      "kreadconfig5", "--file", config_dir + "/kioslaverc", "--group", "Proxy Settings", "--key", "ProxyType"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *enabled = output == "1";

  params = {"kreadconfig5", "--file", config_dir + "/kioslaverc", "--group", "Proxy Settings", "--key", "httpProxy"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *server_addr = output;

  params = {"kreadconfig5", "--file", config_dir + "/kioslaverc", "--group", "Proxy Settings", "--key", "NoProxyFor"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  // trim whites
  if (output[output.size() - 1] == '\n') {
    output.resize(output.size() - 1);
  }
  *bypass_addr = output;

  return true;
}

bool SetSystemProxy_KDE(bool enable, const std::string& server_addr, const std::string& bypass_addr) {
  std::string config_dir = GetConfigDir();
  std::string _;
  std::vector<std::string> params = {"kwriteconfig5"s, "--file"s,           config_dir + "/kioslaverc"s,
                                     "--group"s,       "Proxy Settings"s,   "--key"s,
                                     "ProxyType"s,     enable ? "1"s : "0"s};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  static const char* kProtocol[] = {
      "httpProxy",
      "httpsProxy",
      "ftpProxy",
      "socksProxy",
  };

  for (const char* protocol : kProtocol) {
    params = {"kwriteconfig5", "--file",   config_dir + "/kioslaverc", "--group", "Proxy Settings", "--key",
              protocol,        server_addr};
    if (ExecuteProcess(params, &_, &_) != 0) {
      return false;
    }
  }

  params = {"kwriteconfig5", "--file",   config_dir + "/kioslaverc", "--group", "Proxy Settings", "--key",
            "NoProxyFor",    bypass_addr};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  params = {"dbus-send", "--type=signal", "/KIO/Scheduler", "org.kde.KIO.Scheduler.reparseSlaveConfiguration",
            "string:''"};
  if (ExecuteProcess(params, &_, &_) != 0) {
    return false;
  }

  return true;
}

Dispatcher::Dispatcher() = default;
Dispatcher::~Dispatcher() {
  Destroy();
}

bool Dispatcher::Init(std::function<void()> callback) {
  if (!callback)
    return false;
#ifdef SOCK_NONBLOCK
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds_) != 0) {
    PLOG(WARNING) << "Dispatcher: socketpair failure";
    return false;
  }
#else
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_) != 0) {
    PLOG(WARNING) << "Dispatcher: socketpair failure";
    return false;
  }
  if (::fcntl(fds_[0], F_SETFD, FD_CLOEXEC) != 0 || ::fcntl(fds_[1], F_SETFD, FD_CLOEXEC) != 0) {
    IGNORE_EINTR(::close(fds_[0]));
    IGNORE_EINTR(::close(fds_[1]));
    PLOG(WARNING) << "Dispatcher: set cloexec file descriptor flags failure";
    return false;
  }
  if (::fcntl(fds_[0], F_SETFL, ::fcntl(fds_[0], F_GETFL) | O_NONBLOCK) != 0 ||
      ::fcntl(fds_[1], F_SETFL, ::fcntl(fds_[1], F_GETFL) | O_NONBLOCK) != 0) {
    IGNORE_EINTR(::close(fds_[0]));
    IGNORE_EINTR(::close(fds_[1]));
    PLOG(WARNING) << "Dispatcher: set non-block file status flags failure";
    return false;
  }
#endif
  GIOChannel* channel = g_io_channel_unix_new(fds_[0]);
  source_ = g_io_create_watch(channel, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR));
  g_io_channel_unref(channel);

  g_source_set_priority(source_, G_PRIORITY_LOW);
  auto read_callback = [](GIOChannel* source, GIOCondition condition, gpointer user_data) -> gboolean {
    auto self = reinterpret_cast<Dispatcher*>(user_data);
    if (condition & G_IO_ERR || condition & G_IO_HUP) {
      LOG(WARNING) << "Dispatcher: " << self << " pipe hup";
      return G_SOURCE_REMOVE;
    }
    return self->ReadCallback();
  };

  g_source_set_callback(source_, G_SOURCE_FUNC((GIOFunc)read_callback), this, nullptr);
  g_source_set_name(source_, "Dispatcher");
  g_source_attach(source_, nullptr);
  g_source_unref(source_);

  callback_ = callback;
  LOG(INFO) << "Dispatcher: " << this << " Inited";
  return true;
}

bool Dispatcher::Destroy() {
  if (!source_)
    return true;
  g_source_destroy(source_);
  source_ = nullptr;
  callback_ = nullptr;
  bool failure_on_close = HANDLE_EINTR(::close(fds_[0])) != 0;
  failure_on_close |= HANDLE_EINTR(::close(fds_[1])) != 0;
  if (failure_on_close) {
    PLOG(WARNING) << "Dispatcher: close failure";
    return false;
  }
  fds_[0] = -1;
  fds_[1] = -1;
  LOG(INFO) << "Dispatcher: " << this << " Destroyed";
  return true;
}

bool Dispatcher::Emit() {
  DCHECK(source_);
  DCHECK_NE(fds_[1], -1);
  VLOG(2) << "Dispatcher: " << this << " Emiting Event";
  char data = 0;
  const char* ptr = reinterpret_cast<char*>(&data);
  size_t size = sizeof(data);
  while (size) {
    int written = ::write(fds_[1], ptr, size);
    DCHECK(written);
    if (written < 0 && (errno == EINTR || errno == EAGAIN))
      continue;
    if (written < 0) {
      PLOG(WARNING) << "Dispatcher: write failure";
      return false;
    }
    ptr += written;
    size -= written;
  }
  return true;
}

bool Dispatcher::ReadCallback() {
  DCHECK(source_);
  DCHECK_NE(fds_[0], -1);
  char data = 0;
  char* ptr = reinterpret_cast<char*>(&data);
  size_t size = sizeof(data);
  while (size) {
    int ret = ::read(fds_[0], ptr, size);
    if (ret < 0 && (errno == EINTR || errno == EAGAIN))
      continue;
    if (ret < 0) {
      PLOG(WARNING) << "Dispatcher: read failure";
      return G_SOURCE_REMOVE;
    }
    if (ret == 0) {
      LOG(WARNING) << "Dispatcher: read eof immaturely";
      return G_SOURCE_REMOVE;
    }
    ptr += ret;
    size -= ret;
  }

  if (!callback_) {
    return G_SOURCE_REMOVE;
  }

  VLOG(2) << "Dispatcher: " << this << " Received Event";

  callback_();

  return G_SOURCE_CONTINUE;
}

#if GLIB_VERSION_MIN_REQUIRED >= (G_ENCODE_VERSION(2, 50))
#define GLIB_HAS_STRUCTURE_LOG
#endif

// FIXME current implementation causes gnome-shell high cpu usage
#undef GLIB_HAS_STRUCTURE_LOG

#ifdef GLIB_HAS_STRUCTURE_LOG
// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define GLIB_LOGGING_EX_INFO(ClassName, ...) ClassName(file, atoi(line), LOGGING_INFO, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_WARNING(ClassName, ...) ClassName(file, atoi(line), LOGGING_WARNING, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_ERROR(ClassName, ...) ClassName(file, atoi(line), LOGGING_ERROR, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_FATAL(ClassName, ...) ClassName(file, atoi(line), LOGGING_FATAL, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_DFATAL(ClassName, ...) ClassName(file, atoi(line), LOGGING_DFATAL, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_DCHECK(ClassName, ...) ClassName(file, atoi(line), LOGGING_DCHECK, ##__VA_ARGS__)

#define GLIB_LOGGING_INFO GLIB_LOGGING_EX_INFO(LogMessage)
#define GLIB_LOGGING_WARNING GLIB_LOGGING_EX_WARNING(LogMessage)
#define GLIB_LOGGING_ERROR GLIB_LOGGING_EX_ERROR(LogMessage)
#define GLIB_LOGGING_FATAL GLIB_LOGGING_EX_FATAL(LogMessage)
#define GLIB_LOGGING_DFATAL GLIB_LOGGING_EX_DFATAL(LogMessage)
#define GLIB_LOGGING_DCHECK GLIB_LOGGING_EX_DCHECK(LogMessage)

#define GLIB_LOG_STREAM(severity) GLIB_LOGGING_##severity.stream()

#define GLIB_LOG(severity) LAZY_STREAM(GLIB_LOG_STREAM(severity), LOG_IS_ON(severity))

static GLogWriterOutput GLibLogWriter(GLogLevelFlags log_level,
                                      const GLogField* fields,
                                      gsize n_fields,
                                      gpointer userdata) {
  const char *key, *value;
  gsize i;
  char* message;
  const char *file = "(?)", *line = "0", *func = "(?)";

  // Follow the freedesktop spec
  // https://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html
  for (i = 0, key = fields[i].key, value = (const char*)fields[i].value; i < n_fields; ++i) {
    if (strcmp(key, "CODE_FILE") == 0)
      file = (const char*)value;
    else if (strcmp(key, "CODE_LINE") == 0)
      line = (const char*)value;
    else if (strcmp(key, "CODE_FUNC") == 0)
      func = (const char*)value;
  }

  message = g_log_writer_format_fields(log_level, fields, n_fields, false);

  GLogLevelFlags always_fatal_flags = g_log_set_always_fatal(G_LOG_LEVEL_MASK);
  g_log_set_always_fatal(always_fatal_flags);
  if (always_fatal_flags & log_level) {
    GLIB_LOG(DFATAL) << func << " " << message;
  } else if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL)) {
    GLIB_LOG(ERROR) << func << " " << message;
  } else if (log_level & (G_LOG_LEVEL_WARNING)) {
    GLIB_LOG(WARNING) << func << " " << message;
  } else if (log_level & (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO)) {
    GLIB_LOG(INFO) << func << " " << message;
  } else if (log_level & (G_LOG_LEVEL_DEBUG)) {
#if DCHECK_IS_ON()
    GLIB_LOG(INFO) << func << " " << message;
#else
    g_free(message);
    return G_LOG_WRITER_UNHANDLED;
#endif
  } else {
    NOTREACHED();
    GLIB_LOG(DFATAL) << func << " " << message;
  }

  g_free(message);

  return G_LOG_WRITER_HANDLED;
}

#endif  // GLIB_HAS_STRUCTURE_LOG

static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer /*userdata*/) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  GLogLevelFlags always_fatal_flags = g_log_set_always_fatal(G_LOG_LEVEL_MASK);
  g_log_set_always_fatal(always_fatal_flags);
  GLogLevelFlags fatal_flags = g_log_set_fatal_mask(log_domain, G_LOG_LEVEL_MASK);
  g_log_set_fatal_mask(log_domain, fatal_flags);
  if ((always_fatal_flags | fatal_flags) & log_level) {
    LOG(DFATAL) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL)) {
    LOG(ERROR) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_WARNING)) {
    LOG(WARNING) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO)) {
    LOG(INFO) << log_domain << ": " << message;
  } else if (log_level & G_LOG_LEVEL_DEBUG) {
#if DCHECK_IS_ON()
    LOG(INFO) << log_domain << ": " << message;
#endif
  } else {
    NOTREACHED();
    LOG(DFATAL) << log_domain << ": " << message;
  }
}

void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* const kLogDomains[] = {nullptr, "Gtk", "Gdk", "GLib", "GLib-GObject"};
  for (auto logDomain : kLogDomains) {
    g_log_set_handler(logDomain,
                      static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL | G_LOG_LEVEL_ERROR |
                                                  G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING),
                      GLibLogHandler, nullptr);
  }

#ifdef GLIB_HAS_STRUCTURE_LOG
  // Register GLib-handled structure logging to go through our logging system.
  g_log_set_writer_func(GLibLogWriter, nullptr, nullptr);
#endif  // GLIB_HAS_STRUCTURE_LOG
}
