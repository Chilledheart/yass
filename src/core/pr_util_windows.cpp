// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/pr_log.hpp"
#include "core/pr_util.hpp"

#ifdef _WIN32
#include <algorithm>
#include <stdio.h>
#include <windows.h>

bool _pr_initialized;

void _PR_ImplicitInitialization() {
  WSADATA wsaData;
  int iResult;

  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != NO_ERROR) {
    fprintf(stderr, "Error at WSAStartup()\n");
  }
  _pr_initialized = true;
}

struct PRFileDesc {
  SOCKET fd;
};

/* winnt.h */
PRStatus _PR_MD_GETSOCKOPT(PRFileDesc *fd, int32_t level, int32_t optname,
                           char *optval, int32_t *optlen) {
  int32_t rv;

  rv = getsockopt(fd->fd, level, optname, optval, optlen);
  if (rv == 0)
    return PR_SUCCESS;
  else {
    _PR_MD_MAP_GETSOCKOPT_ERROR(WSAGetLastError());
    return PR_FAILURE;
  }
}

PRStatus _PR_MD_SETSOCKOPT(PRFileDesc *fd, int32_t level, int32_t optname,
                           const char *optval, int32_t optlen) {
  int32_t rv;

  rv = setsockopt(fd->fd, level, optname, optval, optlen);
  if (rv == 0)
    return PR_SUCCESS;
  else {
    _PR_MD_MAP_SETSOCKOPT_ERROR(WSAGetLastError());
    return PR_FAILURE;
  }
}

/*
 * Not every platform has all the socket options we want to
 * support.  Some older operating systems such as SunOS 4.1.3
 * don't have the IP multicast socket options.  Win32 doesn't
 * have TCP_MAXSEG.
 *
 * To deal with this problem, we define the missing socket
 * options as _PR_NO_SUCH_SOCKOPT.  _PR_MapOptionName() fails with
 * PR_OPERATION_NOT_SUPPORTED_ERROR if a socket option not
 * available on the platform is requested.
 */

/*
 * Sanity check.  SO_LINGER and TCP_NODELAY should be available
 * on all platforms.  Just to make sure we have included the
 * appropriate header files.  Then any undefined socket options
 * are really missing.
 */

#if !defined(SO_LINGER)
#error "SO_LINGER is not defined"
#endif

#if !defined(TCP_NODELAY)
#error "TCP_NODELAY is not defined"
#endif

/*
 * Make sure the value of _PR_NO_SUCH_SOCKOPT is not
 * a valid socket option.
 */
#define _PR_NO_SUCH_SOCKOPT -1

#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_SNDBUF
#define SO_SNDBUF _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_RCVBUF
#define SO_RCVBUF _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_IF /* set/get IP multicast interface   */
#define IP_MULTICAST_IF _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_TTL /* set/get IP multicast timetolive  */
#define IP_MULTICAST_TTL _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_MULTICAST_LOOP /* set/get IP multicast loopback    */
#define IP_MULTICAST_LOOP _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_ADD_MEMBERSHIP /* add  an IP group membership      */
#define IP_ADD_MEMBERSHIP _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_DROP_MEMBERSHIP /* drop an IP group membership      */
#define IP_DROP_MEMBERSHIP _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_TTL /* set/get IP Time To Live          */
#define IP_TTL _PR_NO_SUCH_SOCKOPT
#endif

#ifndef IP_TOS /* set/get IP Type Of Service       */
#define IP_TOS _PR_NO_SUCH_SOCKOPT
#endif

#ifndef TCP_NODELAY /* don't delay to coalesce data     */
#define TCP_NODELAY _PR_NO_SUCH_SOCKOPT
#endif

#ifndef TCP_MAXSEG /* maxumum segment size for tcp     */
#define TCP_MAXSEG _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_BROADCAST /* enable broadcast on UDP sockets  */
#define SO_BROADCAST _PR_NO_SUCH_SOCKOPT
#endif

#ifndef SO_REUSEPORT /* allow local address & port reuse */
#define SO_REUSEPORT _PR_NO_SUCH_SOCKOPT
#endif

PRStatus _PR_MapOptionName(PRSockOption optname, int32_t *level,
                           int32_t *name) {
  static int32_t socketOptions[P_SockOpt_Last] = {0,
                                                  SO_LINGER,
                                                  SO_REUSEADDR,
                                                  SO_KEEPALIVE,
                                                  SO_RCVBUF,
                                                  SO_SNDBUF,
                                                  IP_TTL,
                                                  IP_TOS,
                                                  IP_ADD_MEMBERSHIP,
                                                  IP_DROP_MEMBERSHIP,
                                                  IP_MULTICAST_IF,
                                                  IP_MULTICAST_TTL,
                                                  IP_MULTICAST_LOOP,
                                                  TCP_NODELAY,
                                                  TCP_MAXSEG,
                                                  SO_BROADCAST,
                                                  SO_REUSEPORT};
  static int32_t socketLevels[P_SockOpt_Last] = {
      0,          SOL_SOCKET,  SOL_SOCKET,  SOL_SOCKET, SOL_SOCKET, SOL_SOCKET,
      IPPROTO_IP, IPPROTO_IP,  IPPROTO_IP,  IPPROTO_IP, IPPROTO_IP, IPPROTO_IP,
      IPPROTO_IP, IPPROTO_TCP, IPPROTO_TCP, SOL_SOCKET, SOL_SOCKET};

  if ((optname < P_SockOpt_Linger) || (optname >= P_SockOpt_Last)) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return PR_FAILURE;
  }

  if (socketOptions[optname] == _PR_NO_SUCH_SOCKOPT) {
    PR_SetError(PR_OPERATION_NOT_SUPPORTED_ERROR, 0);
    return PR_FAILURE;
  }
  *name = socketOptions[optname];
  *level = socketLevels[optname];
  return PR_SUCCESS;
} /* _PR_MapOptionName */

PRFileDesc *PR_NewUDPSocket(void) {
  if (!_pr_initialized)
    _PR_ImplicitInitialization();

  SOCKET sd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sd >= 0) {
    PRFileDesc *socketFD = new PRFileDesc();
    socketFD->fd = sd;
    return socketFD;
  }
  return nullptr;
}

PRFileDesc *PR_NewTCPSocket(void) {
  if (!_pr_initialized)
    _PR_ImplicitInitialization();

  SOCKET sd = socket(PF_INET, SOCK_STREAM, 0);
  if (sd >= 0) {
    PRFileDesc *socketFD = new PRFileDesc();
    socketFD->fd = sd;
    return socketFD;
  }
  return nullptr;
}

PRFileDesc *PR_OpenUDPSocket(int af) {
  if (!_pr_initialized)
    _PR_ImplicitInitialization();

  SOCKET sd = socket(af, SOCK_DGRAM, 0);
  if (sd >= 0) {
    PRFileDesc *socketFD = new PRFileDesc();
    socketFD->fd = sd;
    return socketFD;
  }
  return nullptr;
}

PRFileDesc *PR_OpenTCPSocket(int af) {
  if (!_pr_initialized)
    _PR_ImplicitInitialization();

  SOCKET sd = socket(af, SOCK_STREAM, 0);
  if (sd >= 0) {
    PRFileDesc *socketFD = new PRFileDesc();
    socketFD->fd = sd;
    return socketFD;
  }
  return nullptr;
}

PRStatus PR_Connect(PRFileDesc *socketFD, const PNetAddr *addr,
                    int /*timeout*/) {
  int addrlen = PNetAddrGetLen(addr);
  if (connect(socketFD->fd, (const struct sockaddr *)addr, addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRFileDesc *PR_Accept(PRFileDesc *socketFD, PNetAddr *addr, int /*timeout*/) {
  PNetAddr ad;
  int addrlen = sizeof(ad);
  SOCKET sd = accept(socketFD->fd, (struct sockaddr *)&ad, &addrlen);
  if (sd >= 0) {
    if (addr) {
      *addr = ad;
    }
    PRFileDesc *newSocketFD = new PRFileDesc;
    newSocketFD->fd = sd;
  }
  return NULL;
}

PRStatus PR_Bind(PRFileDesc *socketFD, const PNetAddr *addr) {
  int addrlen = PNetAddrGetLen(addr);
  if (bind(socketFD->fd, (const struct sockaddr *)addr, addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_Listen(PRFileDesc *socketFd, int backlog) {
  if (listen(socketFd->fd, backlog) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_Shutdown(PRFileDesc *socketFD, PRShutdownHow how) {
  int show = 0;

  switch (how) {
  case PR_SHUTDOWN_RCV:
    show = SD_RECEIVE;
    break;
  case PR_SHUTDOWN_SEND:
    show = SD_SEND;
    break;
  case PR_SHUTDOWN_BOTH:
    show = SD_BOTH;
    break;
  }
  if (shutdown(socketFD->fd, show) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

int32_t PR_Recv(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                int /*timeout*/) {
  int sflags = 0;
  if (flags == PR_MSG_PEEK) {
    sflags = MSG_PEEK;
  }
  return recv(fd->fd, reinterpret_cast<char *>(buf), amount, sflags);
}

int32_t PR_Send(PRFileDesc *fd, const void *buf, int32_t amount, int /*flags*/,
                int /*timeout*/) {
  return send(fd->fd, reinterpret_cast<const char *>(buf), amount, 0);
}

int32_t PR_RecvFrom(PRFileDesc *fd, void *buf, int32_t amount, int /*flags*/,
                    PNetAddr *addr, int /*timeout*/) {
  int addrlen = sizeof(*addr);
  return recvfrom(fd->fd, reinterpret_cast<char *>(buf), amount, 0,
                  (struct sockaddr *)addr, &addrlen);
}

int32_t PR_SendTo(PRFileDesc *fd, const void *buf, int32_t amount,
                  int /*flags*/, const PNetAddr *addr, int /*timeout*/) {
  int addrlen = PNetAddrGetLen(addr);
  return sendto(fd->fd, reinterpret_cast<const char *>(buf), amount, 0,
                (const struct sockaddr *)addr, addrlen);
}

PRStatus PR_NewTCPSocketPair(PRFileDesc *f[2]) {
  /*
   * A socket pair is often used for interprocess communication,
   * so we need to make sure neither socket is associated with
   * the I/O completion port; otherwise it can't be used by a
   * child process.
   *
   * The default implementation below cannot be used for NT
   * because PR_Accept would have associated the I/O completion
   * port with the listening and accepted sockets.
   */
  SOCKET listenSock;
  SOCKET osfd[2];
  struct sockaddr_in selfAddr, peerAddr;
  int addrLen;

  if (!_pr_initialized)
    _PR_ImplicitInitialization();

  osfd[0] = osfd[1] = INVALID_SOCKET;
  listenSock = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSock == INVALID_SOCKET) {
    goto failed;
  }
  selfAddr.sin_family = AF_INET;
  selfAddr.sin_port = 0;
  selfAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* BugZilla: 35408 */
  addrLen = sizeof(selfAddr);
  if (bind(listenSock, (struct sockaddr *)&selfAddr, addrLen) == SOCKET_ERROR) {
    goto failed;
  }
  if (getsockname(listenSock, (struct sockaddr *)&selfAddr, &addrLen) ==
      SOCKET_ERROR) {
    goto failed;
  }
  if (listen(listenSock, 5) == SOCKET_ERROR) {
    goto failed;
  }
  osfd[0] = socket(AF_INET, SOCK_STREAM, 0);
  if (osfd[0] == INVALID_SOCKET) {
    goto failed;
  }
  selfAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  /*
   * Only a thread is used to do the connect and accept.
   * I am relying on the fact that connect returns
   * successfully as soon as the connect request is put
   * into the listen queue (but before accept is called).
   * This is the behavior of the BSD socket code.  If
   * connect does not return until accept is called, we
   * will need to create another thread to call connect.
   */
  if (connect(osfd[0], (struct sockaddr *)&selfAddr, addrLen) == SOCKET_ERROR) {
    goto failed;
  }
  /*
   * A malicious local process may connect to the listening
   * socket, so we need to verify that the accepted connection
   * is made from our own socket osfd[0].
   */
  if (getsockname(osfd[0], (struct sockaddr *)&selfAddr, &addrLen) ==
      SOCKET_ERROR) {
    goto failed;
  }
  osfd[1] = accept(listenSock, (struct sockaddr *)&peerAddr, &addrLen);
  if (osfd[1] == INVALID_SOCKET) {
    goto failed;
  }
  if (peerAddr.sin_port != selfAddr.sin_port) {
    /* the connection we accepted is not from osfd[0] */
    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
    goto failed;
  }
  closesocket(listenSock);

  f[0]->fd = osfd[0];
  f[1]->fd = osfd[1];
  return PR_SUCCESS;

failed:
  if (listenSock != INVALID_SOCKET) {
    closesocket(listenSock);
  }
  if (osfd[0] != INVALID_SOCKET) {
    closesocket(osfd[0]);
  }
  if (osfd[1] != INVALID_SOCKET) {
    closesocket(osfd[1]);
  }
  return PR_FAILURE;
}

PRStatus PR_GetSockName(PRFileDesc *socketFD, PNetAddr *addr) {
  int addrlen = sizeof(*addr);
  if (getsockname(socketFD->fd, (struct sockaddr *)addr, &addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_GetPeerName(PRFileDesc *socketFD, PNetAddr *addr) {
  int addrlen = sizeof(*addr);
  /*
   * NT has a bug that, when invoked on a socket accepted by
   * AcceptEx(), getpeername() returns an all-zero peer address.
   */
  if (getpeername(socketFD->fd, (struct sockaddr *)addr, &addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_GetSocketOption(PRFileDesc *socketFD, PRSocketOptionData *data) {
  PRStatus rv;
  int32_t length;
  int32_t level, name;

  /*
   * P_SockOpt_Nonblocking is a special case that does not
   * translate to a getsockopt() call
   */
  if (P_SockOpt_Nonblocking == data->option) {
    data->value.non_blocking = true;
    return PR_SUCCESS;
  }

  rv = _PR_MapOptionName(data->option, &level, &name);
  if (PR_SUCCESS == rv) {
    switch (data->option) {
    case P_SockOpt_Linger: {
      struct linger linger;
      length = sizeof(linger);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&linger, &length);
      if (PR_SUCCESS == rv) {
        PR_ASSERT(sizeof(linger) == length);
        data->value.linger.polarity = (linger.l_onoff) ? true : false;
        data->value.linger.linger = PR_SecondsToInterval(linger.l_linger);
      }
      break;
    }
    case P_SockOpt_Reuseaddr:
    case P_SockOpt_Keepalive:
    case P_SockOpt_NoDelay:
    case P_SockOpt_Broadcast:
    case P_SockOpt_Reuseport: {
#ifdef _WIN32 /* Winsock */
      bool value;
#else
      int value;
#endif
      length = sizeof(value);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&value, &length);
      if (PR_SUCCESS == rv)
        data->value.reuse_addr = (0 == value) ? false : true;
      break;
    }
    case P_SockOpt_McastLoopback: {
#ifdef _WIN32 /* Winsock */
      bool value;
#else
      uint8_t value;
#endif
      length = sizeof(bool);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&value, &length);
      if (PR_SUCCESS == rv)
        data->value.mcast_loopback = (0 == value) ? false : true;
      break;
    }
    case P_SockOpt_RecvBufferSize:
    case P_SockOpt_SendBufferSize:
    case P_SockOpt_MaxSegment: {
      int value;
      length = sizeof(value);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&value, &length);
      if (PR_SUCCESS == rv)
        data->value.recv_buffer_size = value;
      break;
    }
    case P_SockOpt_IpTimeToLive:
    case P_SockOpt_IpTypeOfService: {
      /* These options should really be an int (or int). */
      length = sizeof(unsigned);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&data->value.ip_ttl,
                             &length);
      break;
    }
    case P_SockOpt_McastTimeToLive: {
#ifdef _WIN32 /* Winsock */
      int ttl;
#else
      uint8_t ttl;
#endif
      length = sizeof(ttl);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&ttl, &length);
      if (PR_SUCCESS == rv)
        data->value.mcast_ttl = ttl;
      break;
    }
#ifdef IP_ADD_MEMBERSHIP
    case P_SockOpt_AddMember:
    case P_SockOpt_DropMember: {
      struct ip_mreq mreq;
      length = sizeof(mreq);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name, (char *)&mreq, &length);
      if (PR_SUCCESS == rv) {
        data->value.add_member.mcaddr.inet.ip = mreq.imr_multiaddr.s_addr;
        data->value.add_member.ifaddr.inet.ip = mreq.imr_interface.s_addr;
      }
      break;
    }
#endif /* IP_ADD_MEMBERSHIP */
    case P_SockOpt_McastInterface: {
      /* This option is a struct in_addr. */
      length = sizeof(data->value.mcast_if.inet.ip);
      rv = _PR_MD_GETSOCKOPT(socketFD, level, name,
                             (char *)&data->value.mcast_if.inet.ip, &length);
      break;
    }
    default:
      PR_NOT_REACHED("Unknown socket option");
      break;
    }
  }
  return rv;
}

PRStatus PR_SetSocketOption(PRFileDesc *socketFD,
                            const PRSocketOptionData *data) {
  PRStatus rv;
  int32_t level, name;

  /*
   * P_SockOpt_Nonblocking is a special case that does not
   * translate to a setsockopt call.
   */
  if (P_SockOpt_Nonblocking == data->option) {
#ifdef _WIN32
    // fd->secret->nonblocking = data->value.non_blocking;
#endif
    return PR_SUCCESS;
  }

  rv = _PR_MapOptionName(data->option, &level, &name);
  if (PR_SUCCESS == rv) {
    switch (data->option) {
    case P_SockOpt_Linger: {
      struct linger linger;
      linger.l_onoff = data->value.linger.polarity;
      linger.l_linger = PR_IntervalToSeconds(data->value.linger.linger);
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&linger,
                             sizeof(linger));
      break;
    }
    case P_SockOpt_Reuseaddr:
    case P_SockOpt_Keepalive:
    case P_SockOpt_NoDelay:
    case P_SockOpt_Broadcast:
    case P_SockOpt_Reuseport: {
#ifdef _WIN32 /* Winsock */
      bool value;
#else
      int value;
#endif
      value = (data->value.reuse_addr) ? 1 : 0;
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&value,
                             sizeof(value));
      break;
    }
    case P_SockOpt_McastLoopback: {
#ifdef _WIN32 /* Winsock */
      bool value;
#else
      uint8_t value;
#endif
      value = data->value.mcast_loopback ? 1 : 0;
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&value,
                             sizeof(value));
      break;
    }
    case P_SockOpt_RecvBufferSize:
    case P_SockOpt_SendBufferSize:
    case P_SockOpt_MaxSegment: {
      int value = data->value.recv_buffer_size;
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&value,
                             sizeof(value));
      break;
    }
    case P_SockOpt_IpTimeToLive:
    case P_SockOpt_IpTypeOfService: {
      /* These options should really be an int (or int). */
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&data->value.ip_ttl,
                             sizeof(unsigned));
      break;
    }
    case P_SockOpt_McastTimeToLive: {
#ifdef _WIN32 /* Winsock */
      int ttl;
#else
      uint8_t ttl;
#endif
      ttl = data->value.mcast_ttl;
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&ttl, sizeof(ttl));
      break;
    }
#ifdef IP_ADD_MEMBERSHIP
    case P_SockOpt_AddMember:
    case P_SockOpt_DropMember: {
      struct ip_mreq mreq;
      mreq.imr_multiaddr.s_addr = data->value.add_member.mcaddr.inet.ip;
      mreq.imr_interface.s_addr = data->value.add_member.ifaddr.inet.ip;
      rv =
          _PR_MD_SETSOCKOPT(socketFD, level, name, (char *)&mreq, sizeof(mreq));
      break;
    }
#endif /* IP_ADD_MEMBERSHIP */
    case P_SockOpt_McastInterface: {
      /* This option is a struct in_addr. */
      rv = _PR_MD_SETSOCKOPT(socketFD, level, name,
                             (char *)&data->value.mcast_if.inet.ip,
                             sizeof(data->value.mcast_if.inet.ip));
      break;
    }
    default:
      PR_NOT_REACHED("Unknown socket option");
      break;
    }
  }
  return rv;
}

PRStatus PR_Close(PRFileDesc *fd) {
  if (closesocket(fd->fd) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

int32_t PR_Read(PRFileDesc *fd, void *buf, int32_t amount) {
  DWORD dwBytesRead;
  bool fRead = ReadFile((HANDLE)fd->fd, buf, amount, &dwBytesRead, NULL);
  if (fRead) {
    return dwBytesRead;
  }
  return -1;
}

int32_t PR_Write(PRFileDesc *fd, const void *buf, int32_t amount) {
  DWORD dwBytesWritten;
  bool fWrite = WriteFile((HANDLE)fd->fd, buf, amount, &dwBytesWritten, NULL);
  if (fWrite) {
    return dwBytesWritten;
  }
  return -1;
}

int32_t PR_Poll(PRPollDesc *pds, int npds, int timeout) {
  int npolls = npds;
  struct timeval tv;

  if (npds != 0) {
    fd_set rfds, wfds, errFds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&errFds);
    tv.tv_sec = PR_IntervalToSeconds(timeout);
    tv.tv_usec =
        PR_IntervalToMicroseconds(timeout - PR_SecondsToInterval(tv.tv_sec));
    int i;
    SOCKET nfd = pds[0].fd->fd;
    for (i = 0; i < npds; ++i) {
      if (pds[i].in_flags & PR_POLL_READ) {
        FD_SET(pds[i].fd->fd, &rfds);
      }
      if (pds[i].in_flags & PR_POLL_WRITE) {
        FD_SET(pds[i].fd->fd, &wfds);
      }
      if (pds[i].in_flags & PR_POLL_ERR) {
        FD_SET(pds[i].fd->fd, &errFds);
      }
      nfd = std::max(pds[i].fd->fd, nfd);
    }
    nfd += 1;
    npolls = select(nfd, &rfds, &wfds, &errFds, &tv);
    for (i = 0; i < npds; ++i) {
      pds[i].out_flags = 0;
      pds[i].out_flags |= FD_ISSET(pds[i].fd->fd, &rfds) ? PR_POLL_READ : 0;
      pds[i].out_flags |= FD_ISSET(pds[i].fd->fd, &wfds) ? PR_POLL_WRITE : 0;
      pds[i].out_flags |= FD_ISSET(pds[i].fd->fd, &errFds) ? PR_POLL_ERR : 0;
    }
  }
  return npolls;
}

#endif
