// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2020 Chilledheart  */

#include "network.hpp"

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <cerrno>

#include "config/config.hpp"
#include "core/logging.hpp"

asio::error_code
SetTCPCongestion(asio::ip::tcp::acceptor::native_handle_type handle) {
  (void)handle;
#if defined(TCP_CONGESTION)
  int fd = handle;
  /* manually enable congestion algorithm */
  char buf[256] = {};
  socklen_t len = sizeof(buf);
  int ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP_CONGESTION is not supported on this platform";
    goto out;
  }
  VLOG(2) << "previous congestion: " << buf;
  len = FLAGS_congestion_algorithm.size();
  ret = setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION,
                   FLAGS_congestion_algorithm.c_str(), len);
  if (ret < 0) {
    VLOG(1) << "Congestion algorithm \"" << FLAGS_congestion_algorithm
            << "\" is not supported on this platform";
    FLAGS_congestion_algorithm = buf;
    goto out;
  }
  len = sizeof(buf);
  ret = getsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, buf, &len);
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP_CONGESTION is not supported on this platform";
    goto out;
  }
  VLOG(2) << "current congestion: " << buf;
out:
#endif // TCP_CONGESTION
  return asio::error_code();
}

asio::error_code
SetTCPFastOpen(asio::ip::tcp::acceptor::native_handle_type handle) {
  if (!FLAGS_tcp_fastopen) {
    return asio::error_code();
  }
  (void)handle;
  // https://docs.microsoft.com/zh-cn/windows/win32/winsock/ipproto-tcp-socket-options?redirectedfrom=MSDN
  // Note that to make use of fast opens, you should use ConnectEx to make the
  // initial connection
#if defined(TCP_FASTOPEN) && !defined(_WIN32)
  int fd = handle;
#ifdef __APPLE__
  int opt = 1; // Apple's iOS 9 and OS X 10.11 both support TCP Fast Open,
               // but it is not enabled for individual connections by default.
               // Public API by using connectx(2)
#else
  int opt = 5; // https://lwn.net/Articles/508865/
               // Value to be chosen by application
#endif // __APPLE__
  int ret = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP Fast Open is not supported on this platform";
    FLAGS_tcp_fastopen = false;
  } else {
    VLOG(2) << "applied current tcp_option: tcp_fastopen";
  }
#endif // TCP_FASTOPEN
  return asio::error_code();
}

asio::error_code
SetTCPFastOpenConnect(asio::ip::tcp::socket::native_handle_type handle) {
  if (!FLAGS_tcp_fastopen_connect) {
    return asio::error_code();
  }
  (void)handle;
#if defined(TCP_FASTOPEN_CONNECT) && !defined(_WIN32)
  // https://android.googlesource.com/kernel/tests/+/master/net/test/tcp_fastopen_test.py
  // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=19f6d3f3c8422d65b5e3d2162e30ef07c6e21ea2
  int fd = handle;
  int opt = 1;
  int ret =
      setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(2) << "TCP Fast Open Connect is not supported on this platform";
    FLAGS_tcp_fastopen_connect = false;
  } else {
    VLOG(2) << "applied current tcp_option: tcp_fastopen_connect";
  }
#endif // TCP_FASTOPEN_CONNECT
  return asio::error_code();
}

asio::error_code
SetTCPUserTimeout(asio::ip::tcp::acceptor::native_handle_type handle) {
  if (!FLAGS_tcp_user_timeout) {
    return asio::error_code();
  }
  (void)handle;
#if defined(TCP_USER_TIMEOUT)
  int fd = handle;
  unsigned int opt = FLAGS_tcp_user_timeout;
  int ret = setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &opt, sizeof(opt));
  if (ret < 0 && (errno == EPROTONOSUPPORT || errno == ENOPROTOOPT)) {
    VLOG(1) << "TCP User Timeout is not supported on this platform";
    FLAGS_tcp_user_timeout = 0;
  } else {
    VLOG(2) << "applied current tcp_option: tcp_user_timeout "
	    << FLAGS_tcp_user_timeout;
  }
#endif // TCP_USER_TIMEOUT
  return asio::error_code();
}

asio::error_code
SetSocketLinger(asio::ip::tcp::socket *socket) {
  if (!FLAGS_so_linger_timeout) {
    return asio::error_code();
  }
  asio::socket_base::linger option(true, FLAGS_so_linger_timeout);
  asio::error_code ec;
  socket->set_option(option, ec);
  if (ec) {
    VLOG(1) << "SO Linger is not supported on this platform: " << ec;
    FLAGS_so_linger_timeout = 0;
  } else {
    VLOG(2) << "Applied SO Linger by " << FLAGS_so_linger_timeout << " seconds";
  }
  return ec;
}
