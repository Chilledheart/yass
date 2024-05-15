// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020-2024 Chilledheart  */

#include "net/network.hpp"

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <mstcpip.h>
#endif

#if !defined(TCP_KEEPIDLE) && defined(TCP_KEEPALIVE)
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif

#include <absl/flags/flag.h>
#include <cerrno>

#include "config/config_network.hpp"
#include "core/logging.hpp"

namespace net {

void SetSOReusePort(asio::ip::tcp::acceptor::native_handle_type handle, asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
  // https://lwn.net/Articles/542629/
  // Please note SO_REUSEADDR is platform-dependent
  // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
#if defined(SO_REUSEPORT)
  int fd = handle;
  int opt = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(2) << "SO_REUSEPORT is not supported on this platform";
  } else {
    VLOG(3) << "Applied current so_option: so_reuseport";
  }
#endif  // SO_REUSEPORT
}

void SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle, asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
#ifdef __linux__
  int fd = handle;
  /* manually enable congestion algorithm */
  char buf[256] = {};
  socklen_t len = sizeof(buf);
  int ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(2) << "TCP_CONGESTION is not supported on this platform";
    ec = asio::error_code(errno, asio::error::get_system_category());
    return;
  }
  if (buf != absl::GetFlag(FLAGS_congestion_algorithm)) {
    len = absl::GetFlag(FLAGS_congestion_algorithm).size();
    ret = setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, absl::GetFlag(FLAGS_congestion_algorithm).c_str(), len);
    if (ret < 0) {
      VLOG(2) << "Congestion algorithm \"" << absl::GetFlag(FLAGS_congestion_algorithm)
              << "\" is not supported on this platform";
      VLOG(2) << "Current congestion: " << buf;
      absl::SetFlag(&FLAGS_congestion_algorithm, buf);
      ec = asio::error_code(errno, asio::error::get_system_category());
      return;
    } else {
      VLOG(3) << "Previous congestion: " << buf;
      VLOG(3) << "Applied current congestion algorithm: " << absl::GetFlag(FLAGS_congestion_algorithm);
    }
  }
  len = sizeof(buf);
  ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP_CONGESTION is not supported on this platform";
    ec = asio::error_code(errno, asio::error::get_system_category());
    return;
  }
  VLOG(3) << "Current congestion: " << buf;
#endif
}

void SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle, asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_tcp_fastopen)) {
    return;
  }
  // https://docs.microsoft.com/zh-cn/windows/win32/winsock/ipproto-tcp-socket-options?redirectedfrom=MSDN
  // Note that to make use of fast opens, you should use ConnectEx to make the
  // initial connection
#if defined(TCP_FASTOPEN) && !defined(_WIN32)
  int fd = handle;
#ifdef __APPLE__
  int opt = 1;  // Apple's iOS 9 and OS X 10.11 both support TCP Fast Open,
                // but it is not enabled for individual connections by default.
                // Public API by using connectx(2)
#else
  int opt = 5;  // https://lwn.net/Articles/508865/
                // Value to be chosen by application
#endif  // __APPLE__
  int ret = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(2) << "TCP Fast Open is not supported on this platform";
    absl::SetFlag(&FLAGS_tcp_fastopen, false);
  } else {
    VLOG(3) << "Applied current tcp_option: tcp_fastopen";
  }
#endif  // TCP_FASTOPEN
}

void SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle, asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_tcp_fastopen_connect)) {
    return;
  }
#if defined(TCP_FASTOPEN_CONNECT) && !defined(_WIN32)
  // https://android.googlesource.com/kernel/tests/+/master/net/test/tcp_fastopen_test.py
  // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=19f6d3f3c8422d65b5e3d2162e30ef07c6e21ea2
  int fd = handle;
  int opt = 1;
  int ret = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(2) << "TCP Fast Open Connect is not supported on this platform";
    absl::SetFlag(&FLAGS_tcp_fastopen_connect, false);
  } else {
    VLOG(3) << "Applied current tcp_option: tcp_fastopen_connect";
  }
#endif  // TCP_FASTOPEN_CONNECT
}

void SetTCPKeepAlive(asio::ip::tcp::acceptor::native_handle_type handle, asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
  int fd = handle;
  unsigned int opt = absl::GetFlag(FLAGS_tcp_keep_alive) ? 1 : 0;
#ifdef _WIN32
  int ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
  int ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
#endif
#ifdef _WIN32
  if (ret < 0) {
    ec = asio::error_code(WSAGetLastError(), asio::error::get_system_category());
#else
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
#endif
    VLOG(2) << "TCP Keep Alive is not supported on this platform " << ec;
    return;
  } else {
    VLOG(3) << "Applied SO socket_option: so_keepalive " << absl::GetFlag(FLAGS_tcp_keep_alive);
  }
  if (!absl::GetFlag(FLAGS_tcp_keep_alive)) {
    return;
  }
#ifdef _WIN32
  struct tcp_keepalive {
    u_long onoff;
    u_long keepalivetime;
    u_long keepaliveinterval;
  };
  tcp_keepalive optVals;
  DWORD cbBytesReturned = 0;
  optVals.onoff = opt;
  optVals.keepalivetime = 1000 * absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
  optVals.keepaliveinterval = 1000 * absl::GetFlag(FLAGS_tcp_keep_alive_interval);
  ret = WSAIoctl(handle, SIO_KEEPALIVE_VALS, &optVals, sizeof(optVals), nullptr, 0, &cbBytesReturned, nullptr, nullptr);
  if (ret < 0) {
    ec = asio::error_code(WSAGetLastError(), asio::error::get_system_category());
    VLOG(2) << "TCP Keep Alive Vals is not supported on this platform: " << ec;
  } else {
    VLOG(3) << "Applied current tcp_option: tcp_keep_alive_idle_timeout "
            << absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
    VLOG(3) << "Applied current tcp_option: tcp_keep_alive_interval " << absl::GetFlag(FLAGS_tcp_keep_alive_interval);
  }
#else
  fd = handle;
  opt = absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
  ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt));
  opt = absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
  ret += setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
  opt = absl::GetFlag(FLAGS_tcp_keep_alive_interval);
  ret += setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(2) << "TCP Keep Alive is not supported on this platform";
  } else {
    VLOG(3) << "Applied current tcp_option: tcp_keep_alive_cnt " << absl::GetFlag(FLAGS_tcp_keep_alive_cnt);
    VLOG(3) << "Applied current tcp_option: tcp_keep_alive_idle_timeout "
            << absl::GetFlag(FLAGS_tcp_keep_alive_idle_timeout);
    VLOG(3) << "Applied current tcp_option: tcp_keep_alive_interval " << absl::GetFlag(FLAGS_tcp_keep_alive_interval);
  }
#endif
}

void SetSocketTcpNoDelay(asio::ip::tcp::socket* socket, asio::error_code& ec) {
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_tcp_nodelay)) {
    return;
  }
  asio::ip::tcp::no_delay option(true);
  socket->set_option(option, ec);
  if (ec) {
    VLOG(2) << "TCP_NODELAY is not supported on this platform: " << ec;
    absl::SetFlag(&FLAGS_tcp_nodelay, false);
  } else {
    VLOG(3) << "Applied TCP_NODELAY";
  }
}

}  // namespace net
