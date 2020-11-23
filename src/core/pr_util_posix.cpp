// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#include "core/pr_util.hpp"

#ifndef _WIN32

#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

bool _pr_initialized;

void _PR_ImplicitInitialization() { _pr_initialized = true; }

struct PRFileDesc {
  int fd;
};

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

#define _PR_FCNTL_FLAGS O_NONBLOCK

static bool pt_IsFdNonblock(int osfd) {
  int flags;
  flags = fcntl(osfd, F_GETFL, 0);
  return flags & _PR_FCNTL_FLAGS;
}

/*
 * Put a Unix file descriptor in non-blocking mode.
 */
static void pt_MakeFdNonblock(int osfd) {
  int flags;
  flags = fcntl(osfd, F_GETFL, 0);
  flags |= _PR_FCNTL_FLAGS;
  (void)fcntl(osfd, F_SETFL, flags);
}

/*
 * Put a Unix socket fd in non-blocking mode that can
 * ideally be inherited by an accepted socket.
 *
 * Why doesn't pt_MakeFdNonblock do?  This is to deal with
 * the special case of HP-UX.  HP-UX has three kinds of
 * non-blocking modes for sockets: the fcntl() O_NONBLOCK
 * and O_NDELAY flags and ioctl() FIOSNBIO request.  Only
 * the ioctl() FIOSNBIO form of non-blocking mode is
 * inherited by an accepted socket.
 *
 * Other platforms just use the generic pt_MakeFdNonblock
 * to put a socket in non-blocking mode.
 */
#ifdef HPUX
static void pt_MakeSocketNonblock(int osfd) {
  int one = 1;
  (void)ioctl(osfd, FIOSNBIO, &one);
}
#else
#define pt_IsSocketNonblock pt_IsFdNonblock
#define pt_MakeSocketNonblock pt_MakeFdNonblock
#endif

PRFileDesc *PR_NewUDPSocket(void) {
  int sd = socket(PF_INET, SOCK_DGRAM, 0);
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

  int sd = socket(PF_INET, SOCK_STREAM, 0);
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

  int sd = socket(af, SOCK_DGRAM, 0);
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

  int sd = socket(af, SOCK_STREAM, 0);
  if (sd >= 0) {
    PRFileDesc *socketFD = new PRFileDesc();
    socketFD->fd = sd;
    return socketFD;
  }
  return nullptr;
}

PRStatus PR_Connect(PRFileDesc *socketFD, const PNetAddr *addr, int timeout) {
  socklen_t addrlen = PNetAddrGetLen(addr);
  if (connect(socketFD->fd, (const struct sockaddr *)addr, addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRFileDesc *PR_Accept(PRFileDesc *socketFD, PNetAddr *addr, int timeout) {
  PNetAddr ad;
  socklen_t addrlen = sizeof(ad);
  int sd = accept(socketFD->fd, (struct sockaddr *)&ad, &addrlen);
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
  socklen_t addrlen = PNetAddrGetLen(addr);
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
    show = SHUT_RD;
    break;
  case PR_SHUTDOWN_SEND:
    show = SHUT_WR;
    break;
  case PR_SHUTDOWN_BOTH:
    show = SHUT_RDWR;
    break;
  }
  if (shutdown(socketFD->fd, show) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

int32_t PR_Recv(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                int timeout) {
  int sflags = 0;
  if (flags == PR_MSG_PEEK) {
    sflags = MSG_PEEK;
  }
  return recv(fd->fd, buf, amount, sflags);
}

int32_t PR_Send(PRFileDesc *fd, const void *buf, int32_t amount, int flags,
                int timeout) {
  return send(fd->fd, buf, amount, 0);
}

int32_t PR_RecvFrom(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                    PNetAddr *addr, int timeout) {
  socklen_t addrlen = sizeof(*addr);
  return recvfrom(fd->fd, buf, amount, 0, (struct sockaddr *)addr, &addrlen);
}

int32_t PR_SendTo(PRFileDesc *fd, const void *buf, int32_t amount, int flags,
                  const PNetAddr *addr, int timeout) {
  socklen_t addrlen = PNetAddrGetLen(addr);
  return sendto(fd->fd, buf, amount, 0, (const struct sockaddr *)addr, addrlen);
}

PRStatus PR_NewTCPSocketPair(PRFileDesc *fds[2]) {
  if (!_pr_initialized)
    _PR_ImplicitInitialization();

#ifdef HAVE_SOCKETPAIR
  int svector[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, svector) == 0) {
    fds[0]->fd = svector[0];
    fds[1]->fd = svector[1];
    return PR_SUCCESS;
  }
  return PR_FAILURE;
#else
  /*
   * For the platforms that don't have socketpair.
   *
   * Copied from prsocket.c, with the parameter f[] renamed fds[] and the
   * _PR_CONNECT_DOES_NOT_BIND code removed.
   */
  PRFileDesc *listenSock;
  PNetAddr selfAddr, peerAddr;
  uint16_t port;

  fds[0] = fds[1] = NULL;
  listenSock = PR_NewTCPSocket();
  if (listenSock == NULL) {
    goto failed;
  }
  PR_InitializeNetAddr(P_IpAddrLoopback, 0, &selfAddr); /* BugZilla: 35408 */
  if (PR_Bind(listenSock, &selfAddr) == PR_FAILURE) {
    goto failed;
  }
  if (PR_GetSockName(listenSock, &selfAddr) == PR_FAILURE) {
    goto failed;
  }
  port = ntohs(selfAddr.inet.port);
  if (PR_Listen(listenSock, 5) == PR_FAILURE) {
    goto failed;
  }
  fds[0] = PR_NewTCPSocket();
  if (fds[0] == NULL) {
    goto failed;
  }
  PR_InitializeNetAddr(P_IpAddrLoopback, port, &selfAddr);

  /*
   * Only a thread is used to do the connect and accept.
   * I am relying on the fact that PR_Connect returns
   * successfully as soon as the connect request is put
   * into the listen queue (but before PR_Accept is called).
   * This is the behavior of the BSD socket code.  If
   * connect does not return until accept is called, we
   * will need to create another thread to call connect.
   */
  if (PR_Connect(fds[0], &selfAddr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
    goto failed;
  }
  /*
   * A malicious local process may connect to the listening
   * socket, so we need to verify that the accepted connection
   * is made from our own socket fds[0].
   */
  if (PR_GetSockName(fds[0], &selfAddr) == PR_FAILURE) {
    goto failed;
  }
  fds[1] = PR_Accept(listenSock, &peerAddr, PR_INTERVAL_NO_TIMEOUT);
  if (fds[1] == NULL) {
    goto failed;
  }
  if (peerAddr.inet.port != selfAddr.inet.port) {
    /* the connection we accepted is not from fds[0] */
    PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
    goto failed;
  }
  PR_Close(listenSock);
  return PR_SUCCESS;

failed:
  if (listenSock) {
    PR_Close(listenSock);
  }
  if (fds[0]) {
    PR_Close(fds[0]);
  }
  if (fds[1]) {
    PR_Close(fds[1]);
  }
  return PR_FAILURE;
#endif
}

PRStatus PR_GetSockName(PRFileDesc *socketFD, PNetAddr *addr) {
  socklen_t addrlen = sizeof(*addr);
  if (getsockname(socketFD->fd, (struct sockaddr *)addr, &addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_GetPeerName(PRFileDesc *socketFD, PNetAddr *addr) {
  socklen_t addrlen = sizeof(*addr);
  if (getpeername(socketFD->fd, (struct sockaddr *)addr, &addrlen) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

PRStatus PR_GetSocketOption(PRFileDesc *socketFD, PRSocketOptionData *data) {

  int rv;
  socklen_t length;
  int32_t level, name;

  /*
   * P_SockOpt_Nonblocking is a special case that does not
   * translate to a getsockopt() call
   */
  if (P_SockOpt_Nonblocking == data->option) {
    data->value.non_blocking = pt_IsSocketNonblock(socketFD->fd);
    return PR_SUCCESS;
  }

  rv = _PR_MapOptionName(data->option, &level, &name);
  if (PR_SUCCESS == rv) {
    switch (data->option) {
    case P_SockOpt_Linger: {
      struct linger linger;
      length = sizeof(linger);
      rv = getsockopt(socketFD->fd, level, name, (char *)&linger, &length);
      PR_ASSERT((-1 == rv) || (sizeof(linger) == length));
      data->value.linger.polarity = (linger.l_onoff) ? true : false;
      data->value.linger.linger = PR_SecondsToInterval(linger.l_linger);
      break;
    }
    case P_SockOpt_Reuseaddr:
    case P_SockOpt_Keepalive:
    case P_SockOpt_NoDelay:
    case P_SockOpt_Broadcast:
    case P_SockOpt_Reuseport: {
      int value;
      length = sizeof(int);
      rv = getsockopt(socketFD->fd, level, name, (char *)&value, &length);
      PR_ASSERT((-1 == rv) || (sizeof(int) == length));
      data->value.reuse_addr = (0 == value) ? false : true;
      break;
    }
    case P_SockOpt_McastLoopback: {
      uint8_t xbool;
      length = sizeof(xbool);
      rv = getsockopt(socketFD->fd, level, name, (char *)&xbool, &length);
      PR_ASSERT((-1 == rv) || (sizeof(xbool) == length));
      data->value.mcast_loopback = (0 == xbool) ? false : true;
      break;
    }
    case P_SockOpt_RecvBufferSize:
    case P_SockOpt_SendBufferSize:
    case P_SockOpt_MaxSegment: {
      int value;
      length = sizeof(int);
      rv = getsockopt(socketFD->fd, level, name, (char *)&value, &length);
      PR_ASSERT((-1 == rv) || (sizeof(int) == length));
      data->value.recv_buffer_size = value;
      break;
    }
    case P_SockOpt_IpTimeToLive:
    case P_SockOpt_IpTypeOfService: {
      length = sizeof(unsigned);
      rv = getsockopt(socketFD->fd, level, name, (char *)&data->value.ip_ttl,
                      &length);
      PR_ASSERT((-1 == rv) || (sizeof(int) == length));
      break;
    }
    case P_SockOpt_McastTimeToLive: {
      uint8_t ttl;
      length = sizeof(ttl);
      rv = getsockopt(socketFD->fd, level, name, (char *)&ttl, &length);
      PR_ASSERT((-1 == rv) || (sizeof(ttl) == length));
      data->value.mcast_ttl = ttl;
      break;
    }
    case P_SockOpt_AddMember:
    case P_SockOpt_DropMember: {
      struct ip_mreq mreq;
      length = sizeof(mreq);
      rv = getsockopt(socketFD->fd, level, name, (char *)&mreq, &length);
      PR_ASSERT((-1 == rv) || (sizeof(mreq) == length));
      data->value.add_member.mcaddr.inet.ip = mreq.imr_multiaddr.s_addr;
      data->value.add_member.ifaddr.inet.ip = mreq.imr_interface.s_addr;
      break;
    }
    case P_SockOpt_McastInterface: {
      length = sizeof(data->value.mcast_if.inet.ip);
      rv = getsockopt(socketFD->fd, level, name,
                      (char *)&data->value.mcast_if.inet.ip, &length);
      PR_ASSERT((-1 == rv) || (sizeof(data->value.mcast_if.inet.ip) == length));
      break;
    }
    default:
      PR_NOT_REACHED("Unknown socket option");
      break;
    }
    if (-1 == rv)
      _PR_MD_MAP_GETSOCKOPT_ERROR(errno);
  }
  return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}

PRStatus PR_SetSocketOption(PRFileDesc *socketFD,
                            const PRSocketOptionData *data) {

  int rv;
  int32_t level, name;
  /*
   * P_SockOpt_Nonblocking is a special case that does not
   * translate to a setsockopt call.
   */
  if (P_SockOpt_Nonblocking == data->option) {
    pt_MakeSocketNonblock(socketFD->fd);
    return PR_SUCCESS;
  }
  rv = _PR_MapOptionName(data->option, &level, &name);
  if (PR_SUCCESS == rv) {
    switch (data->option) {
    case P_SockOpt_Linger: {
      struct linger linger;
      linger.l_onoff = data->value.linger.polarity;
      linger.l_linger = PR_IntervalToSeconds(data->value.linger.linger);
      rv = setsockopt(socketFD->fd, level, name, (char *)&linger,
                      sizeof(linger));
      break;
    }
    case P_SockOpt_Reuseaddr:
    case P_SockOpt_Keepalive:
    case P_SockOpt_NoDelay:
    case P_SockOpt_Broadcast:
    case P_SockOpt_Reuseport: {
      int value = (data->value.reuse_addr) ? 1 : 0;
      rv = setsockopt(socketFD->fd, level, name, (char *)&value, sizeof(int));
#ifdef LINUX
      /* for pt_LinuxSendFile */
      if (name == TCP_NODELAY && rv == 0) {
      }
#endif
      break;
    }
    case P_SockOpt_McastLoopback: {
      uint8_t xbool = data->value.mcast_loopback ? 1 : 0;
      rv = setsockopt(socketFD->fd, level, name, (char *)&xbool, sizeof(xbool));
      break;
    }
    case P_SockOpt_RecvBufferSize:
    case P_SockOpt_SendBufferSize:
    case P_SockOpt_MaxSegment: {
      int value = data->value.recv_buffer_size;
      rv = setsockopt(socketFD->fd, level, name, (char *)&value, sizeof(int));
      break;
    }
    case P_SockOpt_IpTimeToLive:
    case P_SockOpt_IpTypeOfService: {
      rv = setsockopt(socketFD->fd, level, name, (char *)&data->value.ip_ttl,
                      sizeof(unsigned));
      break;
    }
    case P_SockOpt_McastTimeToLive: {
      uint8_t ttl = data->value.mcast_ttl;
      rv = setsockopt(socketFD->fd, level, name, (char *)&ttl, sizeof(ttl));
      break;
    }
    case P_SockOpt_AddMember:
    case P_SockOpt_DropMember: {
      struct ip_mreq mreq;
      mreq.imr_multiaddr.s_addr = data->value.add_member.mcaddr.inet.ip;
      mreq.imr_interface.s_addr = data->value.add_member.ifaddr.inet.ip;
      rv = setsockopt(socketFD->fd, level, name, (char *)&mreq, sizeof(mreq));
      break;
    }
    case P_SockOpt_McastInterface: {
      rv = setsockopt(socketFD->fd, level, name,
                      (char *)&data->value.mcast_if.inet.ip,
                      sizeof(data->value.mcast_if.inet.ip));
      break;
    }
    default:
      PR_NOT_REACHED("Unknown socket option");
      break;
    }
    if (-1 == rv)
      _PR_MD_MAP_SETSOCKOPT_ERROR(errno);
  }
  return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}

PRStatus PR_Close(PRFileDesc *fd) {
  if (close(fd->fd) == 0) {
    return PR_SUCCESS;
  }
  return PR_FAILURE;
}

int32_t PR_Read(PRFileDesc *fd, void *buf, int32_t amount) {
  return read(fd->fd, buf, amount);
}

int32_t PR_Write(PRFileDesc *fd, const void *buf, int32_t amount) {
  return write(fd->fd, buf, amount);
}

int32_t PR_Poll(PRPollDesc *pds, int npds, int timeout) {
  int npolls = npds;
  if (npds != 0) {
    struct pollfd *fds = new struct pollfd[npds];
    int i;
    for (i = 0; i < npds; ++i) {
      fds[i].fd = pds[i].fd->fd;
      fds[i].events = pds[i].in_flags;
    }
    npolls = poll(fds, npolls, timeout);
    for (i = 0; i < npds; ++i) {
      pds[i].out_flags = fds[i].revents;
    }
  }
  return npolls;
}

#endif // _WIN32
