// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Chilledheart  */

#include "network.hpp"

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <absl/flags/flag.h>
#include <cerrno>

#include "core/logging.hpp"

ABSL_FLAG(bool, reuse_port, true, "Reuse the listening port");
ABSL_FLAG(std::string, congestion_algorithm, "bbr", "TCP Congestion Algorithm");
ABSL_FLAG(bool, tcp_fastopen, false, "TCP fastopen");
ABSL_FLAG(bool, tcp_fastopen_connect, false, "TCP fastopen connect");
ABSL_FLAG(int32_t, connect_timeout, 60, "Connect timeout (Linux only)");
ABSL_FLAG(int32_t, tcp_user_timeout, 300, "TCP user timeout (Linux only)");
ABSL_FLAG(int32_t, so_linger_timeout, 30, "SO Linger timeout");

ABSL_FLAG(int32_t, so_snd_buffer, 16 * 1024, "Socket Send Buffer");
ABSL_FLAG(int32_t, so_rcv_buffer, 128 * 1024, "Socket Receive Buffer");

void SetSOReusePort(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code& ec) {
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
    VLOG(1) << "SO_REUSEPORT is not supported on this platform";
  } else {
    VLOG(2) << "Applied current so_option: so_reuseport";
  }
#endif  // SO_REUSEPORT
}

void SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle,
                      asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
#if defined(TCP_CONGESTION)
  int fd = handle;
  /* manually enable congestion algorithm */
  char buf[256] = {};
  socklen_t len = sizeof(buf);
  int ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP_CONGESTION is not supported on this platform";
    ec = asio::error_code(errno, asio::error::get_system_category());
    return;
  }
  if (buf != absl::GetFlag(FLAGS_congestion_algorithm)) {
    len = absl::GetFlag(FLAGS_congestion_algorithm).size();
    ret = setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION,
                     absl::GetFlag(FLAGS_congestion_algorithm).c_str(), len);
    if (ret < 0) {
      VLOG(1) << "Congestion algorithm \""
              << absl::GetFlag(FLAGS_congestion_algorithm)
              << "\" is not supported on this platform";
      VLOG(1) << "Current congestion: " << buf;
      absl::SetFlag(&FLAGS_congestion_algorithm, buf);
      ec = asio::error_code(errno, asio::error::get_system_category());
      return;
    } else {
      VLOG(2) << "Previous congestion: " << buf;
      VLOG(2) << "Applied current congestion algorithm: "
              << absl::GetFlag(FLAGS_congestion_algorithm);
    }
  }
  len = sizeof(buf);
  ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP_CONGESTION is not supported on this platform";
    ec = asio::error_code(errno, asio::error::get_system_category());
    return;
  }
  VLOG(2) << "Current congestion: " << buf;
#endif  // TCP_CONGESTION
}

void SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle,
                    asio::error_code& ec) {
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
    VLOG(1) << "TCP Fast Open is not supported on this platform";
    absl::SetFlag(&FLAGS_tcp_fastopen, false);
  } else {
    VLOG(2) << "Applied current tcp_option: tcp_fastopen";
  }
#endif  // TCP_FASTOPEN
}

void SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle,
                           asio::error_code& ec) {
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
  int ret =
      setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(2) << "TCP Fast Open Connect is not supported on this platform";
    absl::SetFlag(&FLAGS_tcp_fastopen_connect, false);
  } else {
    VLOG(2) << "Applied current tcp_option: tcp_fastopen_connect";
  }
#endif  // TCP_FASTOPEN_CONNECT
}

void SetTCPUserTimeout(asio::ip::tcp::acceptor::native_handle_type handle,
                       asio::error_code& ec) {
  (void)handle;
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_tcp_user_timeout)) {
    return;
  }
#if defined(TCP_USER_TIMEOUT)
  int fd = handle;
  unsigned int opt = absl::GetFlag(FLAGS_tcp_user_timeout);
  int ret = setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    ec = asio::error_code(errno, asio::error::get_system_category());
    VLOG(1) << "TCP User Timeout is not supported on this platform";
    absl::SetFlag(&FLAGS_tcp_user_timeout, 0);
  } else {
    VLOG(2) << "Applied current tcp_option: tcp_user_timeout "
            << absl::GetFlag(FLAGS_tcp_user_timeout);
  }
#endif  // TCP_USER_TIMEOUT
}

void SetSocketLinger(asio::ip::tcp::socket* socket, asio::error_code& ec) {
  if (!absl::GetFlag(FLAGS_so_linger_timeout)) {
    ec = asio::error_code();
    return;
  }
  asio::socket_base::linger option(true,
                                   absl::GetFlag(FLAGS_so_linger_timeout));
  socket->set_option(option, ec);
  if (ec) {
    VLOG(1) << "SO Linger is not supported on this platform: " << ec;
    absl::SetFlag(&FLAGS_so_linger_timeout, 0);
  } else {
    VLOG(2) << "Applied SO Linger by " << absl::GetFlag(FLAGS_so_linger_timeout)
            << " seconds";
  }
}

void SetSocketSndBuffer(asio::ip::tcp::socket* socket, asio::error_code& ec) {
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_so_snd_buffer)) {
    return;
  }
  asio::socket_base::send_buffer_size option(
      absl::GetFlag(FLAGS_so_snd_buffer));
  socket->set_option(option, ec);
  if (ec) {
    VLOG(1) << "SO_SNDBUF is not supported on this platform: " << ec;
    absl::SetFlag(&FLAGS_so_snd_buffer, 0);
  } else {
    VLOG(2) << "Applied SO_SNDBUF by " << absl::GetFlag(FLAGS_so_snd_buffer)
            << " bytes";
  }
}

void SetSocketRcvBuffer(asio::ip::tcp::socket* socket, asio::error_code& ec) {
  ec = asio::error_code();
  if (!absl::GetFlag(FLAGS_so_rcv_buffer)) {
    return;
  }
  asio::socket_base::receive_buffer_size option(
      absl::GetFlag(FLAGS_so_rcv_buffer));
  socket->set_option(option, ec);
  if (ec) {
    VLOG(1) << "SO_RCVBUF is not supported on this platform: " << ec;
    absl::SetFlag(&FLAGS_so_rcv_buffer, 0);
  } else {
    VLOG(2) << "Applied SO_RCVBUF by " << absl::GetFlag(FLAGS_so_rcv_buffer)
            << " bytes";
  }
}
