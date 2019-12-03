//
// pr_util_posix.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/pr_util.hpp"

#ifndef _WIN32

#include <cstring>
     #include <sys/types.h>
     #include <sys/socket.h>
#include <unistd.h>

bool _pr_initialized;

void _PR_ImplicitInitialization() {
    _pr_initialized = true;
}

struct PRFileDesc {
    int fd;
};

inline uint32_t
PNetAddrGetLen(const PNetAddr *addr) {
    switch (addr->raw.family) {
      case  P_AF_INET:
         return sizeof(addr->inet);
      case  P_AF_INET6:
         return sizeof(addr->ipv6);
         break;
      case  P_AF_LOCAL:
         return sizeof(addr->local.family) + strlen(addr->local.path);
      default:
         return 0;

    }
}

PRFileDesc*    PR_NewUDPSocket(void) {
   int sd = socket(PF_INET, SOCK_DGRAM, 0);
   if (sd >= 0) {
     PRFileDesc* socketFD = new PRFileDesc();
     socketFD->fd = sd;
     return socketFD;
   }
   return nullptr;
}

PRFileDesc*    PR_NewTCPSocket(void) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}

PRFileDesc*    PR_OpenUDPSocket(int af) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    int sd = socket(af, SOCK_DGRAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}

PRFileDesc*    PR_OpenTCPSocket(int af) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    int sd = socket(af, SOCK_STREAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}


PRStatus
PR_Connect(PRFileDesc *socketFD, const PNetAddr *addr,
           int timeout) {
    socklen_t addrlen = PNetAddrGetLen(addr);
    if (connect(socketFD->fd, (const struct sockaddr*)addr, addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRFileDesc*
PR_Accept(PRFileDesc *socketFD, PNetAddr *addr, int timeout) {
    PNetAddr ad;
    socklen_t addrlen = sizeof(ad);
    int sd = accept(socketFD->fd, (struct sockaddr*)&ad, &addrlen);
    if (sd >= 0) {
        if (addr) {
            *addr = ad;
        }
        PRFileDesc *newSocketFD = new PRFileDesc;
        newSocketFD->fd = sd;
    }
    return NULL;
}


PRStatus
PR_Bind(PRFileDesc *socketFD, const PNetAddr *addr) {
    socklen_t addrlen = PNetAddrGetLen(addr);
    if (bind(socketFD->fd, (const struct sockaddr*)addr, addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_Listen(PRFileDesc *socketFd, int backlog) {
    if (listen(socketFd->fd, backlog) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_Shutdown(PRFileDesc *socketFD, PRShutdownHow how) {
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


int32_t
PR_Recv(PRFileDesc *fd, void *buf, int32_t amount,
                int flags, int timeout) {
    int sflags = 0;
    if (flags == PR_MSG_PEEK) {
        sflags = MSG_PEEK;
    }
    return recv(fd->fd, buf, amount, sflags);
}


int32_t
PR_Send(PRFileDesc *fd, const void *buf, int32_t amount,
                int flags, int timeout) {
    return send(fd->fd, buf, amount, 0);
}


int32_t
PR_RecvFrom(
    PRFileDesc *fd, void *buf, int32_t amount, int flags,
    PNetAddr *addr, int timeout) {
    socklen_t addrlen = sizeof(*addr);
    return recvfrom(fd->fd, buf, amount, 0,
        (struct sockaddr*)addr, &addrlen);
}


int32_t
PR_SendTo(
    PRFileDesc *fd, const void *buf, int32_t amount, int flags,
    const PNetAddr *addr, int timeout) {
    socklen_t addrlen = PNetAddrGetLen(addr);
    return sendto(fd->fd, buf, amount, 0,
        (const struct sockaddr*)addr, addrlen);
}


PRStatus
PR_NewTCPSocketPair(PRFileDesc *fds[2]) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

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
    if (PR_Connect(fds[0], &selfAddr, PR_INTERVAL_NO_TIMEOUT)
            == PR_FAILURE) {
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


PRStatus
PR_GetSockName(PRFileDesc *socketFD, const PNetAddr *addr) {
    socklen_t addrlen = sizeof(*addr);
    if (getsockname(socketFD->fd, (struct sockaddr*)addr, &addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_GetPeerName(PRFileDesc *socketFD, const PNetAddr *addr) {
    socklen_t addrlen = sizeof(*addr);
    if (getpeername(socketFD->fd, (struct sockaddr*)addr, &addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_GetSocketOption(PRFileDesc *socketFD, int option, void *option_value) {

    socklen_t option_value_len = sizeof(char);
    if (getsockopt(socketFD->fd, SOL_SOCKET, option, option_value, &option_value_len) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_SetSocketOption(PRFileDesc *socketFD, int option, const void *option_value) {

    socklen_t option_value_len = sizeof(char);
    if (setsockopt(socketFD->fd, SOL_SOCKET, option, option_value, option_value_len) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_Close(PRFileDesc *fd) {
    if (close(fd->fd) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


int32_t
PR_Read(PRFileDesc *fd, void *buf, int32_t amount) {
    return read(fd->fd, buf, amount);
}


int32_t
PR_Write(PRFileDesc *fd, const void *buf, int32_t amount) {
    return write(fd->fd, buf, amount);
}


int32_t PR_Poll(
    PRPollDesc *pds, int npds, int timeout) {
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
