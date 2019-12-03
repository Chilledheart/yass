//
// pr_util_windows.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "core/pr_util.hpp"

#ifdef _WIN32
#include <algorithm>
#include <windows.h>
#include <stdio.h>

bool _pr_initialized;

void _PR_ImplicitInitialization() {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != NO_ERROR) {
      fprintf(stderr, "Error at WSAStartup()\n");
    }
    _pr_initialized = true;
}

struct PRFileDesc {
    SOCKET fd;
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
    if (!_pr_initialized) _PR_ImplicitInitialization();

    SOCKET sd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}

PRFileDesc*    PR_NewTCPSocket(void) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    SOCKET sd = socket(PF_INET, SOCK_STREAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}

PRFileDesc*    PR_OpenUDPSocket(int af) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    SOCKET sd = socket(af, SOCK_DGRAM, 0);
    if (sd >= 0) {
      PRFileDesc* socketFD = new PRFileDesc();
      socketFD->fd = sd;
      return socketFD;
    }
    return nullptr;
}

PRFileDesc*    PR_OpenTCPSocket(int af) {
    if (!_pr_initialized) _PR_ImplicitInitialization();

    SOCKET sd = socket(af, SOCK_STREAM, 0);
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
    int addrlen = PNetAddrGetLen(addr);
    if (connect(socketFD->fd, (const struct sockaddr*)addr, addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRFileDesc*
PR_Accept(PRFileDesc *socketFD, PNetAddr *addr, int timeout) {
    PNetAddr ad;
    int addrlen = sizeof(ad);
    SOCKET sd = accept(socketFD->fd, (struct sockaddr*)&ad, &addrlen);
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
    int addrlen = PNetAddrGetLen(addr);
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


int32_t
PR_Recv(PRFileDesc *fd, void *buf, int32_t amount,
                int flags, int timeout) {
    int sflags = 0;
    if (flags == PR_MSG_PEEK) {
        sflags = MSG_PEEK;
    }
    return recv(fd->fd, reinterpret_cast<char*>(buf), amount, sflags);
}


int32_t
PR_Send(PRFileDesc *fd, const void *buf, int32_t amount,
                int flags, int timeout) {
    return send(fd->fd, reinterpret_cast<const char*>(buf), amount, 0);
}


int32_t
PR_RecvFrom(
    PRFileDesc *fd, void *buf, int32_t amount, int flags,
    PNetAddr *addr, int timeout) {
    int addrlen = sizeof(*addr);
    return recvfrom(fd->fd, reinterpret_cast<char*>(buf), amount, 0,
        (struct sockaddr*)addr, &addrlen);
}


int32_t
PR_SendTo(
    PRFileDesc *fd, const void *buf, int32_t amount, int flags,
    const PNetAddr *addr, int timeout) {
    int addrlen = PNetAddrGetLen(addr);
    return sendto(fd->fd, reinterpret_cast<const char*>(buf), amount, 0,
        (const struct sockaddr*)addr, addrlen);
}


PRStatus
PR_NewTCPSocketPair(PRFileDesc *f[2]) {
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

    if (!_pr_initialized) _PR_ImplicitInitialization();

    osfd[0] = osfd[1] = INVALID_SOCKET;
    listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == INVALID_SOCKET) {
        goto failed;
    }
    selfAddr.sin_family = AF_INET;
    selfAddr.sin_port = 0;
    selfAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* BugZilla: 35408 */
    addrLen = sizeof(selfAddr);
    if (bind(listenSock, (struct sockaddr *) &selfAddr,
            addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    if (getsockname(listenSock, (struct sockaddr *) &selfAddr,
            &addrLen) == SOCKET_ERROR) {
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
    if (connect(osfd[0], (struct sockaddr *) &selfAddr,
            addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    /*
     * A malicious local process may connect to the listening
     * socket, so we need to verify that the accepted connection
     * is made from our own socket osfd[0].
     */
    if (getsockname(osfd[0], (struct sockaddr *) &selfAddr,
            &addrLen) == SOCKET_ERROR) {
        goto failed;
    }
    osfd[1] = accept(listenSock, (struct sockaddr *) &peerAddr, &addrLen);
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


PRStatus
PR_GetSockName(PRFileDesc *socketFD, const PNetAddr *addr) {
    int addrlen = sizeof(*addr);
    if (getsockname(socketFD->fd, (struct sockaddr*)addr, &addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_GetPeerName(PRFileDesc *socketFD, const PNetAddr *addr) {
    int addrlen = sizeof(*addr);
    /*
     * NT has a bug that, when invoked on a socket accepted by
     * AcceptEx(), getpeername() returns an all-zero peer address.
     */
    if (getpeername(socketFD->fd, (struct sockaddr*)addr, &addrlen) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_GetSocketOption(PRFileDesc *socketFD, int option, void *option_value) {
    int option_value_len = sizeof(char);
    if (getsockopt(socketFD->fd, SOL_SOCKET, option, reinterpret_cast<char*>(option_value),
          &option_value_len) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_SetSocketOption(PRFileDesc *socketFD, int option, const void *option_value) {
    int option_value_len = sizeof(char);
    if (setsockopt(socketFD->fd, SOL_SOCKET, option, reinterpret_cast<const char*>(option_value),
          option_value_len) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


PRStatus
PR_Close(PRFileDesc *fd) {
    if (closesocket(fd->fd) == 0) {
        return PR_SUCCESS;
    }
    return PR_FAILURE;
}


int32_t
PR_Read(PRFileDesc *fd, void *buf, int32_t amount) {
    DWORD dwBytesRead;
    bool fRead = ReadFile((HANDLE)fd->fd, buf, amount, &dwBytesRead, NULL);
    if (fRead) {
        return dwBytesRead;
    }
    return -1;
}


int32_t
PR_Write(PRFileDesc *fd, const void *buf, int32_t amount) {
    DWORD dwBytesWritten;
    bool fWrite = WriteFile((HANDLE)fd->fd, buf, amount, &dwBytesWritten, NULL);
    if (fWrite) {
        return dwBytesWritten;
    }
    return -1;
}


int32_t PR_Poll(
    PRPollDesc *pds, int npds, int timeout) {
    int npolls = npds;
    struct timeval tv;

    if (npds != 0) {
        fd_set rfds, wfds, errFds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&errFds);
        tv.tv_sec = PR_IntervalToSeconds(timeout);
        tv.tv_usec = PR_IntervalToMicroseconds(
            timeout - PR_SecondsToInterval(tv.tv_sec));
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

