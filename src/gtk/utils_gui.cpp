// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "gtk/utils.hpp"

#include "core/logging.hpp"

#include <absl/strings/string_view.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "core/cxx17_backports.hpp"

// Available in 2.58
#ifndef G_SOURCE_FUNC
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))
#endif

static const char* kAutoStartFileContent =
"[Desktop Entry]\n"
"Type=Application\n"
"Name=yass\n"
"Comment=Yet Another Shadow Socket is a lightweight and secure http/socks4/socks5 proxy for embedded devices and low end boxes.\n"
"Icon=yass\n"
"Exec=yass --background\n"
"Terminal=false\n"
"Categories=Network;GTK;Utility\n";

namespace {

std::string ExpandUser(const std::string& file_path) {
  std::string real_path = file_path;

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home = ::getenv("HOME");
    return home + "/" + real_path.substr(2);
  }

  return real_path;
}

// returns true if the "file" exists and is a symbolic link
bool IsFile(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISREG(Stat.st_mode);
    }
  }
  return S_ISREG(Stat.st_mode);
}

// returns true if the "dir" exists and is a symbolic link
bool IsDirectory(const std::string& path) {
  struct stat Stat;
  if (::stat(path.c_str(), &Stat) != 0) {
    return false;
  }
  if (S_ISLNK(Stat.st_mode)) {
    char real_path[PATH_MAX];
    if (char* resolved_path = ::realpath(path.c_str(), real_path)) {
      return ::stat(resolved_path, &Stat) == 0 && S_ISDIR(Stat.st_mode);
    }
  }
  return S_ISDIR(Stat.st_mode);
}

bool CreatePrivateDirectory(const std::string& path) {
  return mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) != 0;
}

bool EnsureCreatedDirectory(const std::string& path) {
  if (!IsDirectory(path) && !CreatePrivateDirectory(path)) {
    return false;
  }
  return true;
}

bool WriteFileWithContent(const std::string& path, absl::string_view context) {
  int fd = open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return false;
  }
  ssize_t ret = write(fd, context.data(), context.size());

  if (ret != static_cast<long>(context.size()) || close(fd) < 0) {
    return false;
  }
  return true;
}

std::string GetAutostartDirectory() {
  const char* XdgConfigHome = getenv("XDG_CONFIG_HOME");
  if (!XdgConfigHome)
    XdgConfigHome = "~/.config";
  return ExpandUser(XdgConfigHome) + "/" + "autostart";
}
}  // namespace

bool Utils::GetAutoStart() {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  return IsFile(autostart_desktop);
}

void Utils::EnableAutoStart(bool on) {
  std::string autostart_desktop =
    GetAutostartDirectory() + "/" + DEFAULT_AUTOSTART_NAME + ".desktop";
  if (!on) {
    if (::unlink(autostart_desktop.c_str()) != 0) {
      PLOG(WARNING)
          << "Internal error: unable to remove autostart file";
    }
  } else {

    EnsureCreatedDirectory(GetAutostartDirectory());
    // force update, delete first
    if (IsFile(autostart_desktop) && ::unlink(autostart_desktop.c_str()) != 0) {
      PLOG(WARNING)
          << "Internal error: unable to remove previous autostart file";
    }

    // write to target
    if (!WriteFileWithContent(autostart_desktop, kAutoStartFileContent))  {
      PLOG(WARNING) << "Internal error: unable to create autostart file";
    }
  }
}

Dispatcher::Dispatcher() = default;
Dispatcher::~Dispatcher() {
  Destroy();
}

bool Dispatcher::Init(std::function<void()> callback) {
  if (!callback)
    return false;
#ifdef SOCK_NONBLOCK
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                 fds_) != 0) {
    PLOG(WARNING) << "Dispatcher: socketpair failure";
    return false;
  }
#else
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds_) != 0) {
    PLOG(WARNING) << "Dispatcher: socketpair failure";
    return false;
  }
  if (fcntl(fds_[0], F_SETFL, fcntl(fds_[0], F_GETFL) | O_NONBLOCK | O_CLOEXEC) != 0 ||
      fcntl(fds_[1], F_SETFL, fcntl(fds_[1], F_GETFL) | O_NONBLOCK | O_CLOEXEC) != 0) {
    close(fds_[0]);
    close(fds_[1]);
    PLOG(WARNING) << "Dispatcher: set non-block failure";
    return false;
  }
#endif
  GIOChannel* channel = g_io_channel_unix_new(fds_[0]);
  source_ =
      g_io_create_watch(channel, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR));
  g_io_channel_unref(channel);

  g_source_set_priority(source_, G_PRIORITY_LOW);
  auto read_callback = [](GIOChannel* source, GIOCondition condition,
                          gpointer user_data) -> gboolean {
    auto self = reinterpret_cast<Dispatcher*>(user_data);
    if (condition & G_IO_ERR || condition & G_IO_HUP) {
      VLOG(2) << "Dispatcher: " << self << " pipe hup";
      self->Destroy();
      return G_SOURCE_REMOVE;
    }
    return self->ReadCallback();
  };

  g_source_set_callback(source_, G_SOURCE_FUNC((GIOFunc)read_callback), this,
                        nullptr);
  g_source_set_name(source_, "Dispatcher");
  g_source_attach(source_, nullptr);
  g_source_unref(source_);

  callback_ = callback;
  VLOG(2) << "Dispatcher: " << this << " Inited";
  return true;
}

bool Dispatcher::Destroy() {
  if (!source_)
    return true;
  g_source_destroy(source_);
  source_ = nullptr;
  if (close(fds_[0]) != 0 || close(fds_[1]) != 0) {
    PLOG(WARNING) << "Dispatcher: close failure";
    return false;
  }
  fds_[0] = -1;
  fds_[1] = -1;
  VLOG(2) << "Dispatcher: " << this << " Destroyed";
  return true;
}

bool Dispatcher::Emit() {
  DCHECK(source_);
  DCHECK_NE(fds_[1], -1);
  VLOG(2) << "Dispatcher: " << this << " Emiting";
  int32_t data = 0;
  const char* ptr = reinterpret_cast<char*>(&data);
  uint32_t size = sizeof(data);
  while (size) {
    int ret = write(fds_[1], ptr, size);
    if (ret < 0 && (errno == EINTR || errno == EAGAIN))
      continue;
    if (ret < 0) {
      PLOG(WARNING) << "Dispatcher: write failure";
      return false;
    }
    ptr += ret;
    size -= ret;
  }
  return true;
}

bool Dispatcher::ReadCallback() {
  DCHECK(source_);
  DCHECK_NE(fds_[0], -1);

  VLOG(2) << "Dispatcher: " << this << " Reading";

  int32_t data = 0;
  char* ptr = reinterpret_cast<char*>(&data);
  uint32_t size = sizeof(data);
  while (size) {
    int ret = read(fds_[0], ptr, size);
    if (ret < 0 && (errno == EINTR || errno == EAGAIN))
      continue;
    if (ret < 0) {
      PLOG(WARNING) << "Dispatcher: read failure";
      return G_SOURCE_REMOVE;
    }
    ptr += ret;
    size -= ret;
  }

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
#define GLIB_LOGGING_EX_INFO(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_INFO, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_WARNING(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_WARNING, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_ERROR(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_ERROR, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_FATAL(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_FATAL, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_DFATAL(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_DFATAL, ##__VA_ARGS__)
#define GLIB_LOGGING_EX_DCHECK(ClassName, ...) \
  ClassName(file, atoi(line), LOGGING_DCHECK, ##__VA_ARGS__)

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
  char *message;
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

#endif // GLIB_HAS_STRUCTURE_LOG

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
  GLogLevelFlags fatal_flags =
      g_log_set_fatal_mask(log_domain, G_LOG_LEVEL_MASK);
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
  const char* const kLogDomains[] = {nullptr, "Gtk", "Gdk", "GLib",
                                     "GLib-GObject"};
  for (auto logDomain : kLogDomains) {
    g_log_set_handler(
        logDomain,
        static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL |
                                    G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                                    G_LOG_LEVEL_WARNING),
        GLibLogHandler, nullptr);
  }

#ifdef GLIB_HAS_STRUCTURE_LOG
  // Register GLib-handled structure logging to go through our logging system.
  g_log_set_writer_func(GLibLogWriter, nullptr, nullptr);
#endif // GLIB_HAS_STRUCTURE_LOG
}
