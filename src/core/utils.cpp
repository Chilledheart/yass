// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#include "core/utils.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#ifdef HAVE_TCMALLOC
#include <tcmalloc/malloc_extension.h>
#endif

#include <absl/flags/flag.h>
#include <absl/flags/internal/program_name.h>
#include <absl/strings/str_cat.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <iomanip>
#include <sstream>
#include "url/gurl.h"

#include "config/config.hpp"
#include "core/logging.hpp"

#ifdef __ANDROID__
std::string a_cache_dir;
std::string a_data_dir;
OpenApkAssetType a_open_apk_asset = nullptr;
#endif

#ifdef __OHOS__
std::string h_cache_dir;
std::string h_data_dir;
#endif

std::optional<int> StringToInteger(std::string_view value) {
  int result;

  if (gurl_base::StringToInt(value, &result)) {
    return result;
  }

  return std::nullopt;
}

std::optional<unsigned> StringToIntegerU(std::string_view value) {
  unsigned result;

  if (gurl_base::StringToUint(value, &result)) {
    return result;
  }

  return std::nullopt;
}

std::optional<int64_t> StringToInteger64(std::string_view value) {
  int64_t result;

  if (gurl_base::StringToInt64(value, &result)) {
    return result;
  }

  return std::nullopt;
}

std::optional<uint64_t> StringToIntegerU64(std::string_view value) {
  uint64_t result;

  if (gurl_base::StringToUint64(value, &result)) {
    return result;
  }

  return std::nullopt;
}

#ifdef _WIN32
const char kSeparators[] = "/\\";
#else
const char kSeparators[] = "/";
#endif

std::string_view Dirname(std::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.empty() ? "/" : path.substr(0, 1);
  }

  auto last_slash_pos = path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory.
  if (last_slash_pos == std::string_view::npos) {
    return ".";
  }

  // trim the extra trailing slash
  first_non_slash_at_end_pos = path.find_last_not_of(kSeparators, last_slash_pos);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.substr(0, 1);
  }

  return path.substr(0, first_non_slash_at_end_pos + 1);
}

// A portable interface that returns the basename of the filename passed as an
// argument. It is similar to basename(3)
// <https://linux.die.net/man/3/basename>.
// For example:
//     Basename("a/b/prog/file.cc")
// returns "file.cc"
//     Basename("a/b/prog//")
// returns "prog"
//     Basename("file.cc")
// returns "file.cc"
//     Basename("/file.cc")
// returns "file.cc"
//     Basename("//file.cc")
// returns "file.cc"
//     Basename("/dir//file.cc")
// returns "file.cc"
//     Basename("////")
// returns "/"
//     Basename("c/")
// returns "c"
//     Basename("/a/b/c")
// returns "c"
//
// TODO: handle with driver letter under windows
std::string_view Basename(std::string_view path) {
  // trim the extra trailing slash
  auto first_non_slash_at_end_pos = path.find_last_not_of(kSeparators);

  // path is in the root directory
  if (first_non_slash_at_end_pos == std::string_view::npos) {
    return path.empty() ? "" : path.substr(0, 1);
  }

  auto last_slash_pos = path.find_last_of(kSeparators, first_non_slash_at_end_pos);

  // path is in the current directory
  if (last_slash_pos == std::string_view::npos) {
    return path.substr(0, first_non_slash_at_end_pos + 1);
  }

  // path is in the root directory
  return path.substr(last_slash_pos + 1, first_non_slash_at_end_pos - last_slash_pos);
}

std::string ExpandUser(std::string_view file_path) {
  std::string real_path = std::string(file_path);

  if (!real_path.empty() && real_path[0] == '~') {
    std::string home;
    {
      const char* home_str = ::getenv("HOME");
      if (home_str) {
        home = home_str;
      }
    }
#ifdef __ANDROID__
    if (!a_data_dir.empty()) {
      home = a_data_dir;
    }
#endif
    if (home.empty()) {
#ifdef _WIN32
      home = absl::StrCat(::getenv("HOMEDRIVE"), ::getenv("HOMEPATH"));
#else
      struct passwd pwd;
      struct passwd* result = nullptr;
      char buffer[PATH_MAX * 2] = {'\0'};
      uid_t uid = ::geteuid();
      int pwuid_res = ::getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &result);
      if (pwuid_res == 0 && result) {
        home = pwd.pw_dir;
      } else {
        home = "/";
      }
#endif
    }
    if (real_path.size() == 1) {
      return home;
    }
    // ~username
    if (real_path[1] != '/') {
#ifdef _WIN32
      return absl::StrCat(::getenv("HOMEDRIVE"), "\\Users", "\\", real_path.substr(1));
#else
      struct passwd pwd;
      struct passwd* result = nullptr;
      char buffer[PATH_MAX * 2] = {'\0'};
      auto pos = real_path.find("/", 1);
      auto username = real_path.substr(1, pos - 1);
      pos = real_path.find_first_not_of("/", pos);
      int pwnam_res = ::getpwnam_r(username.c_str(), &pwd, buffer, sizeof(buffer), &result);
      if (pwnam_res != 0 || result == nullptr) {
        return "/";
      }
      if (pos != std::string::npos) {
        return absl::StrCat(pwd.pw_dir, "/", real_path.substr(pos).c_str());
      } else {
        return pwd.pw_dir;
      }
#endif
    }
    // ~/path/to/directory
    return absl::StrCat(home,
#ifdef _WIN32
                        "\\",
#else
                        "/",
#endif
                        real_path.substr(2));
  }

  return real_path;
}

#if !defined(__APPLE__) && !defined(_WIN32)

static constexpr std::string_view kDefaultExePath = "UNKNOWN";
static std::string main_exe_path = std::string(kDefaultExePath);

bool GetExecutablePath(std::string* path) {
  char exe_path[PATH_MAX];
  ssize_t ret = readlink("/proc/self/exe", exe_path, sizeof(exe_path));
  if (ret >= 0) {
    *path = std::string(exe_path, ret);
    return true;
  }
  *path = main_exe_path;
  return true;
}

void SetExecutablePath(const std::string& exe_path) {
  main_exe_path = exe_path;

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}

bool GetTempDir(std::string* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp) {
    *path = tmp;
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  // return PathService::Get(DIR_CACHE, path);
  if (!a_cache_dir.empty()) {
    *path = a_cache_dir;
    return true;
  }
#endif
#if BUILDFLAG(IS_OHOS)
  if (!h_cache_dir.empty()) {
    *path = h_cache_dir;
    return true;
  }
#endif
#if defined(__ANDROID) || defined(__OHOS__)
  *path = "/data/local/tmp";
#else
  *path = "/tmp";
#endif
  return true;
}

std::string GetHomeDir() {
  const char* home_dir = getenv("HOME");
  if (home_dir && home_dir[0]) {
    return home_dir;
  }
#if defined(__ANDROID__)
  DLOG(WARNING) << "OS_ANDROID: Home directory lookup not yet implemented.";
#endif
  // Fall back on temp dir if no home directory is defined.
  std::string rv;
  if (GetTempDir(&rv)) {
    return rv;
  }
  // Last resort.
  return "/tmp";
}
#endif

/*
 * Net_ipv6works() returns true if IPv6 seems to work.
 */
bool Net_ipv6works() {
  if (!absl::GetFlag(FLAGS_ipv6_mode)) {
    return false;
  }
#ifdef _WIN32
  using fd_t = SOCKET;
#else
  using fd_t = int;
#endif
  /* probe to see if we have a working IPv6 stack */
  fd_t s = ::socket(AF_INET6, SOCK_DGRAM, 0);
#ifndef _WIN32
  if (s < 0) {
#else
  if (s == INVALID_SOCKET) {
#endif
    return false;
  } else {
#ifndef _WIN32
    IGNORE_EINTR(::close(s));
#else
    ::closesocket(s);
#endif
    return true;
  }
}

#ifndef _WIN32
ssize_t ReadFileToBuffer(const std::string& path, char* buf, size_t buf_len) {
  int fd = HANDLE_EINTR(::open(path.c_str(), O_RDONLY));
  if (fd < 0) {
    return -1;
  }
  ssize_t ret = HANDLE_EINTR(::read(fd, buf, buf_len - 1));

  if (HANDLE_EINTR(close(fd)) < 0) {
    return -1;
  }
  buf[ret] = '\0';
  return ret;
}

static bool WriteFileDescriptor(int fd, std::string_view data) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  ssize_t size = static_cast<ssize_t>(data.size());
  DCHECK_LE(data.size(), static_cast<size_t>(SSIZE_MAX));  // checked_cast
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size; bytes_written_total += bytes_written_partial) {
    bytes_written_partial =
        HANDLE_EINTR(::write(fd, data.data() + bytes_written_total, static_cast<size_t>(size - bytes_written_total)));
    if (bytes_written_partial < 0)
      return false;
  }
  return true;
}

ssize_t WriteFileWithBuffer(const std::string& path, std::string_view buf) {
  int fd = HANDLE_EINTR(::open(path.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  if (fd < 0) {
    return false;
  }

  ssize_t ret = WriteFileDescriptor(fd, buf) ? buf.length() : -1;

  if (HANDLE_EINTR(close(fd)) < 0) {
    return -1;
  }
  return ret;
}

PlatformFile OpenReadFile(const std::string& path) {
  return HANDLE_EINTR(::open(path.c_str(), O_RDONLY));
}
#endif

#ifndef _WIN32
bool IsProgramConsole(int fd) {
  return isatty(fd) == 1;
}
#endif

#ifdef HAVE_TCMALLOC
void PrintTcmallocStats() {
  std::vector<const char*> properties = {
      "generic.current_allocated_bytes",       "generic.heap_size",
      "tcmalloc.max_total_thread_cache_bytes", "tcmalloc.current_total_thread_cache_bytes",
      "tcmalloc.pageheap_free_bytes",          "tcmalloc.pageheap_unmapped_bytes",
  };
  for (auto property : properties) {
    absl::optional<size_t> size = tcmalloc::MallocExtension::GetNumericProperty(property);
    if (size.has_value()) {
      LOG(INFO) << "TCMALLOC: " << property << " = " << *size << " bytes";
    }
  }
}
#endif

template <typename T>
static void HumanReadableByteCountBinT(T* ss, uint64_t bytes) {
  if (bytes < 1024) {
    *ss << bytes << " B";
    return;
  }
  uint64_t value = bytes;
  char ci[] = {"KMGTPE"};
  const char* c = ci;
  for (int i = 40; i >= 0 && bytes > 0xfffccccccccccccLU >> i; i -= 10) {
    value >>= 10;
    ++c;
  }
  *ss << std::fixed << std::setw(5) << std::setprecision(2) << static_cast<double>(value) / 1024.0 << " " << *c;
}

void HumanReadableByteCountBin(std::ostream* ss, uint64_t bytes) {
  HumanReadableByteCountBinT(ss, bytes);
}

#ifdef _WIN32
void HumanReadableByteCountBin(std::wostream* ss, uint64_t bytes) {
  HumanReadableByteCountBinT(ss, bytes);
}
#endif

const char* ProgramTypeToStr(ProgramType type) {
  switch (type) {
    case YASS_CLIENT:
      return "client";
    case YASS_SERVER:
      return "server";
    case YASS_CLIENT_SLAVE:
      return "client (slave)";
    case YASS_UNITTEST:
      return "unittest";
    case YASS_BENCHMARK:
      return "benchmark";
    case YASS_UNSPEC:
    default:
      return "unspec";
  }
}

template <int DefaultPort>
bool SplitHostPortWithDefaultPort(std::string* out_hostname,
                                  uint16_t* out_port,
                                  const std::string& host_port_string) {
  url::Component username_component;
  url::Component password_component;
  url::Component host_component;
  url::Component port_component;

  url::ParseAuthority(host_port_string.data(), url::Component(0, host_port_string.size()), &username_component,
                      &password_component, &host_component, &port_component);

  // Only support "host", "host:port" and nothing more or less.
  if (username_component.is_valid() || password_component.is_valid() || !host_component.is_nonempty()) {
    DVLOG(1) << "HTTP authority could not be parsed: " << host_port_string;
    return false;
  }

  std::string_view hostname(host_port_string.data() + host_component.begin, host_component.len);
  std::string_view port;
  if (!port_component.is_empty()) {
    port = std::string_view(host_port_string.data() + port_component.begin, port_component.len);
  }

  int parsed_port_number =
      port_component.is_empty() ? DefaultPort : url::ParsePort(host_port_string.data(), port_component);
  // Negative result is either invalid or unspecified, either of which is
  // disallowed for this parse. Port 0 is technically valid but reserved and not
  // really usable in practice, so easiest to just disallow it here.
  if (parsed_port_number <= 0 || parsed_port_number > UINT16_MAX) {
    DVLOG(1) << "Port could not be parsed while parsing from: " << port;
    return false;
  }
  *out_hostname = hostname;
  *out_port = static_cast<uint16_t>(parsed_port_number);
  return true;
}

template bool SplitHostPortWithDefaultPort<80>(std::string* out_hostname,
                                               uint16_t* out_port,
                                               const std::string& host_port_string);

template bool SplitHostPortWithDefaultPort<443>(std::string* out_hostname,
                                                uint16_t* out_port,
                                                const std::string& host_port_string);
