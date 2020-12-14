// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef PR_UTIL
#define PR_UTIL

#include "core/pr_error.hpp"

/* prtypes.h */
#include <stdint.h>
/*
** Status code used by some routines that have a single point of failure or
** special status return.
*/
typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

/* prthread.h */
typedef struct PRThread PRThread;
/*
** Return the current thread object for the currently running code.
** Never returns NULL.
*/
PRThread *PR_GetCurrentThread(void);

/* prlog.h */
#if defined(_DEBUG) || defined(FORCE_PR_ASSERT)

#define PR_ASSERT(_expr)                                                       \
  ((_expr) ? ((void)0) : PR_Assert(#_expr, __FILE__, __LINE__))

#define PR_NOT_REACHED(_reasonStr) PR_Assert(_reasonStr, __FILE__, __LINE__)

#else

#define PR_ASSERT(expr) ((void)0)
#define PR_NOT_REACHED(reasonStr)

#endif /* defined(_DEBUG) || defined(FORCE_PR_ASSERT) */

/* prinrval.h */
/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

typedef uint32_t PRIntervalTime;

/***********************************************************************
** DEFINES:     PR_INTERVAL_MIN
**              PR_INTERVAL_MAX
** DESCRIPTION:
**  These two constants define the range (in ticks / second) of the
**  platform dependent type, PRIntervalTime. These constants bound both
**  the period and the resolution of a PRIntervalTime.
***********************************************************************/
#define PR_INTERVAL_MIN 1000UL
#define PR_INTERVAL_MAX 100000UL

/***********************************************************************
** DEFINES:     PR_INTERVAL_NO_WAIT
**              PR_INTERVAL_NO_TIMEOUT
** DESCRIPTION:
**  Two reserved constants are defined in the PRIntervalTime namespace.
**  They are used to indicate that the process should wait no time (return
**  immediately) or wait forever (never time out), respectively.
**  Note: PR_INTERVAL_NO_TIMEOUT passed as input to PR_Connect is
**  interpreted as use the OS's connect timeout.
**
***********************************************************************/
#define PR_INTERVAL_NO_WAIT 0UL
#define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL

/**********************************************************************/
/****************************** FUNCTIONS *****************************/
/**********************************************************************/

/***********************************************************************
** FUNCTION:    PR_IntervalNow
** DESCRIPTION:
**  Return the value of NSPR's free running interval timer. That timer
**  can be used to establish epochs and determine intervals (be computing
**  the difference between two times).
** INPUTS:      void
** OUTPUTS:     void
** RETURN:      PRIntervalTime
**
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  The units of PRIntervalTime are platform dependent. They are chosen
**  such that they are appropriate for the host OS, yet provide sufficient
**  resolution and period to be useful to clients.
** MEMORY:      N/A
** ALGORITHM:   Platform dependent
***********************************************************************/
PRIntervalTime PR_IntervalNow(void);

/***********************************************************************
** FUNCTION:    PR_TicksPerSecond
** DESCRIPTION:
**  Return the number of ticks per second for PR_IntervalNow's clock.
**  The value will be in the range [PR_INTERVAL_MIN..PR_INTERVAL_MAX].
** INPUTS:      void
** OUTPUTS:     void
** RETURN:      uint32_t
**
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  None
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
uint32_t PR_TicksPerSecond(void);

/***********************************************************************
** FUNCTION:    PR_SecondsToInterval
**              PR_MillisecondsToInterval
**              PR_MicrosecondsToInterval
** DESCRIPTION:
**  Convert standard clock units to platform dependent intervals.
** INPUTS:      uint32_t
** OUTPUTS:     void
** RETURN:      PRIntervalTime
**
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  Conversion may cause overflow, which is not reported.
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
PRIntervalTime PR_SecondsToInterval(uint32_t seconds);
PRIntervalTime PR_MillisecondsToInterval(uint32_t milli);
PRIntervalTime PR_MicrosecondsToInterval(uint32_t micro);

/***********************************************************************
** FUNCTION:    PR_IntervalToSeconds
**              PR_IntervalToMilliseconds
**              PR_IntervalToMicroseconds
** DESCRIPTION:
**  Convert platform dependent intervals to standard clock units.
** INPUTS:      PRIntervalTime
** OUTPUTS:     void
** RETURN:      uint32_t
**
** SIDE EFFECTS:
**  None
** RESTRICTIONS:
**  Conversion may cause overflow, which is not reported.
** MEMORY:      N/A
** ALGORITHM:   N/A
***********************************************************************/
uint32_t PR_IntervalToSeconds(PRIntervalTime ticks);
uint32_t PR_IntervalToMilliseconds(PRIntervalTime ticks);
uint32_t PR_IntervalToMicroseconds(PRIntervalTime ticks);

/* prtime.h */
/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

#define PR_MSEC_PER_SEC 1000L
#define PR_USEC_PER_SEC 1000000L
#define PR_NSEC_PER_SEC 1000000000L
#define PR_USEC_PER_MSEC 1000L
#define PR_NSEC_PER_MSEC 1000000L

/* prio.h */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h> /* INADDR_ANY, ..., ntohl(), ... */
#define P_AF_INET 2
#define P_AF_LOCAL 1
#define P_INADDR_ANY (unsigned long)0x00000000
#define P_INADDR_LOOPBACK 0x7f000001
#define P_INADDR_BROADCAST (unsigned long)0xffffffff
#else
#include <netinet/in.h> /* INADDR_ANY, ..., ntohl(), ... */
#include <sys/socket.h> /* AF_INET */
#include <sys/types.h>
#define P_AF_INET AF_INET
#define P_AF_LOCAL AF_UNIX
#define P_INADDR_ANY INADDR_ANY
#define P_INADDR_LOOPBACK INADDR_LOOPBACK
#define P_INADDR_BROADCAST INADDR_BROADCAST
#endif

#ifndef _WIN32
#define P_AF_INET6 AF_INET6
#define P_AF_UNSPEC AF_UNSPEC
#endif

#ifndef P_AF_INET6
#define P_AF_INET6 100
#endif

#ifndef P_AF_UNSPEC
#define P_AF_UNSPEC 0
#endif

/*
**************************************************************************
** A network address
**
** Only Internet Protocol (IPv4 and IPv6) addresses are supported.
** The address family must always represent IPv4 (AF_INET, probably == 2)
** or IPv6 (AF_INET6).
**************************************************************************
*************************************************************************/

struct PIPv6Addr {
  union {
    uint8_t _S6_u8[16];
    uint16_t _S6_u16[8];
    uint32_t _S6_u32[4];
    uint64_t _S6_u64[2];
  } _S6_un;
};
#define p_s6_addr _S6_un._S6_u8
#define p_s6_addr16 _S6_un._S6_u16
#define p_s6_addr32 _S6_un._S6_u32
#define p_s6_addr64 _S6_un._S6_u64

typedef struct PIPv6Addr PIPv6Addr;

union PNetAddr {
  struct {
    uint16_t family; /* address family (0x00ff maskable) */
    char data[14];   /* raw address data */
  } raw;
  struct {
    uint16_t family; /* address family (AF_INET) */
    uint16_t port;   /* port number */
    uint32_t ip;     /* The actual 32 bits of address */
    char pad[8];
  } inet;
  struct {
    uint16_t family;   /* address family (AF_INET6) */
    uint16_t port;     /* port number */
    uint32_t flowinfo; /* routing information */
    PIPv6Addr ip;      /* the actual 128 bits of address */
    uint32_t scope_id; /* set of interfaces for a scope */
  } ipv6;
  struct {           /* Unix domain socket address */
    uint16_t family; /* address family (AF_UNIX) */
    char path[104];  /* null-terminated pathname */
  } local;
};

#include <cstring>

inline uint32_t PNetAddrGetLen(const PNetAddr *addr) {
  switch (addr->raw.family) {
  case P_AF_INET:
    return sizeof(addr->inet);
  case P_AF_INET6:
    return sizeof(addr->ipv6);
    break;
  case P_AF_LOCAL:
    return sizeof(addr->local.family) + strlen(addr->local.path);
  default:
    return 0;
  }
}

inline bool PNetAddrCmp(const PNetAddr *lhs, const PNetAddr *rhs) {
  return PNetAddrGetLen(lhs) == PNetAddrGetLen(rhs) &&
         memcmp(lhs, &rhs, PNetAddrGetLen(lhs)) == 0;
}

/*
***************************************************************************
** PRSockOption
**
** The file descriptors can have predefined options set after they file
** descriptor is created to change their behavior. Only the options in
** the following enumeration are supported.
***************************************************************************
*/
typedef enum PRSockOption {
  P_SockOpt_Nonblocking,    /* nonblocking io */
  P_SockOpt_Linger,         /* linger on close if data present */
  P_SockOpt_Reuseaddr,      /* allow local address reuse */
  P_SockOpt_Keepalive,      /* keep connections alive */
  P_SockOpt_RecvBufferSize, /* receive buffer size */
  P_SockOpt_SendBufferSize, /* send buffer size */

  P_SockOpt_IpTimeToLive,    /* time to live */
  P_SockOpt_IpTypeOfService, /* type of service and precedence */

  P_SockOpt_AddMember,       /* add an IP group membership */
  P_SockOpt_DropMember,      /* drop an IP group membership */
  P_SockOpt_McastInterface,  /* multicast interface address */
  P_SockOpt_McastTimeToLive, /* multicast timetolive */
  P_SockOpt_McastLoopback,   /* multicast loopback */

  P_SockOpt_NoDelay,    /* don't delay send to coalesce packets */
  P_SockOpt_MaxSegment, /* maximum segment size */
  P_SockOpt_Broadcast,  /* enable broadcast */
  P_SockOpt_Reuseport,  /* allow local address & port reuse on
                         * platforms that support it */
  P_SockOpt_Last
} PRSockOption;

typedef struct PRLinger {
  bool polarity;         /* Polarity of the option's setting */
  PRIntervalTime linger; /* Time to linger before closing */
} PRLinger;

typedef struct PRMcastRequest {
  PNetAddr mcaddr; /* IP multicast address of group */
  PNetAddr ifaddr; /* local IP address of interface */
} PRMcastRequest;

typedef struct PRSocketOptionData {
  PRSockOption option;
  union {
    unsigned int ip_ttl;        /* IP time to live */
    unsigned int mcast_ttl;     /* IP multicast time to live */
    unsigned int tos;           /* IP type of service and precedence */
    bool non_blocking;          /* Non-blocking (network) I/O */
    bool reuse_addr;            /* Allow local address reuse */
    bool reuse_port;            /* Allow local address & port reuse on
                                 * platforms that support it */
    bool keep_alive;            /* Keep connections alive */
    bool mcast_loopback;        /* IP multicast loopback */
    bool no_delay;              /* Don't delay send to coalesce packets */
    bool broadcast;             /* Enable broadcast */
    size_t max_segment;         /* Maximum segment size */
    size_t recv_buffer_size;    /* Receive buffer size */
    size_t send_buffer_size;    /* Send buffer size */
    PRLinger linger;            /* Time to linger on close if data present */
    PRMcastRequest add_member;  /* add an IP group membership */
    PRMcastRequest drop_member; /* Drop an IP group membership */
    PNetAddr mcast_if;          /* multicast interface address */
  } value;
} PRSocketOptionData;

typedef struct PRFileDesc PRFileDesc;

/*
 *************************************************************************
 * FUNCTION: PR_NewUDPSocket
 * DESCRIPTION:
 *     Create a new UDP socket.
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_NewUDPSocket returns a pointer
 *     to the PRFileDesc created for the newly opened UDP socket.
 *     Returns a NULL pointer if the creation of a new UDP socket failed.
 *
 **************************************************************************
 */

PRFileDesc *PR_NewUDPSocket(void);

/*
 *************************************************************************
 * FUNCTION: PR_NewTCPSocket
 * DESCRIPTION:
 *     Create a new TCP socket.
 * INPUTS:
 *     None
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_NewTCPSocket returns a pointer
 *     to the PRFileDesc created for the newly opened TCP socket.
 *     Returns a NULL pointer if the creation of a new TCP socket failed.
 *
 **************************************************************************
 */

PRFileDesc *PR_NewTCPSocket(void);

/*
 *************************************************************************
 * FUNCTION: PR_OpenUDPSocket
 * DESCRIPTION:
 *     Create a new UDP socket of the specified address family.
 * INPUTS:
 *     int af
 *       Address family
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_OpenUDPSocket returns a pointer
 *     to the PRFileDesc created for the newly opened UDP socket.
 *     Returns a NULL pointer if the creation of a new UDP socket failed.
 *
 **************************************************************************
 */

PRFileDesc *PR_OpenUDPSocket(int af);

/*
 *************************************************************************
 * FUNCTION: PR_OpenTCPSocket
 * DESCRIPTION:
 *     Create a new TCP socket of the specified address family.
 * INPUTS:
 *     int af
 *       Address family
 * OUTPUTS:
 *     None
 * RETURN: PRFileDesc*
 *     Upon successful completion, PR_NewTCPSocket returns a pointer
 *     to the PRFileDesc created for the newly opened TCP socket.
 *     Returns a NULL pointer if the creation of a new TCP socket failed.
 *
 **************************************************************************
 */

PRFileDesc *PR_OpenTCPSocket(int af);

/*
 *************************************************************************
 * FUNCTION: PR_Connect
 * DESCRIPTION:
 *     Initiate a connection on a socket.
 * INPUTS:
 *     PRFileDesc *fd
 *       Points to a PRFileDesc object representing a socket
 *     PNetAddr *addr
 *       Specifies the address of the socket in its own communication
 *       space.
 *     int timeout
 *       The function uses the lesser of the provided timeout and
 *       the OS's connect timeout.  In particular, if you specify
 *       PR_INTERVAL_NO_TIMEOUT as the timeout, the OS's connection
 *       time limit will be used.
 *
 * OUTPUTS:
 *     None
 * RETURN: PRStatus
 *     Upon successful completion of connection initiation, PR_Connect
 *     returns PR_SUCCESS.  Otherwise, it returns PR_FAILURE.  Further
 *     failure information can be obtained by calling PR_GetError().
 **************************************************************************
 */

PRStatus PR_Connect(PRFileDesc *socketFD, const PNetAddr *addr, int timeout);

/*
 *************************************************************************
 * FUNCTION: PR_Accept
 * DESCRIPTION:
 *     Accept a connection on a socket.
 * INPUTS:
 *     PRFileDesc *fd
 *       Points to a PRFileDesc object representing the rendezvous socket
 *       on which the caller is willing to accept new connections.
 *     int timeout
 *       Time limit for completion of the accept operation.
 * OUTPUTS:
 *     PNetAddr *addr
 *       Returns the address of the connecting entity in its own
 *       communication space. It may be NULL.
 * RETURN: PRFileDesc*
 *     Upon successful acceptance of a connection, PR_Accept
 *     returns a valid file descriptor. Otherwise, it returns NULL.
 *     Further failure information can be obtained by calling PR_GetError().
 **************************************************************************
 */

PRFileDesc *PR_Accept(PRFileDesc *fd, PNetAddr *addr, int timeout);

/*
 *************************************************************************
 * FUNCTION: PR_Bind
 * DESCRIPTION:
 *    Bind an address to a socket.
 * INPUTS:
 *     PRFileDesc *fd
 *       Points to a PRFileDesc object representing a socket.
 *     PNetAddr *addr
 *       Specifies the address to which the socket will be bound.
 * OUTPUTS:
 *     None
 * RETURN: PRStatus
 *     Upon successful binding of an address to a socket, PR_Bind
 *     returns PR_SUCCESS.  Otherwise, it returns PR_FAILURE.  Further
 *     failure information can be obtained by calling PR_GetError().
 **************************************************************************
 */

PRStatus PR_Bind(PRFileDesc *fd, const PNetAddr *addr);

/*
 *************************************************************************
 * FUNCTION: PR_Listen
 * DESCRIPTION:
 *    Listen for connections on a socket.
 * INPUTS:
 *     PRFileDesc *fd
 *       Points to a PRFileDesc object representing a socket that will be
 *       used to listen for new connections.
 *     int backlog
 *       Specifies the maximum length of the queue of pending connections.
 * OUTPUTS:
 *     None
 * RETURN: PRStatus
 *     Upon successful completion of listen request, PR_Listen
 *     returns PR_SUCCESS.  Otherwise, it returns PR_FAILURE.  Further
 *     failure information can be obtained by calling PR_GetError().
 **************************************************************************
 */

PRStatus PR_Listen(PRFileDesc *fd, int backlog);

/*
 *************************************************************************
 * FUNCTION: PR_Shutdown
 * DESCRIPTION:
 *    Shut down part of a full-duplex connection on a socket.
 * INPUTS:
 *     PRFileDesc *fd
 *       Points to a PRFileDesc object representing a connected socket.
 *     int how
 *       Specifies the kind of disallowed operations on the socket.
 *           PR_SHUTDOWN_RCV - Further receives will be disallowed
 *           PR_SHUTDOWN_SEND - Further sends will be disallowed
 *           PR_SHUTDOWN_BOTH - Further sends and receives will be disallowed
 * OUTPUTS:
 *     None
 * RETURN: PRStatus
 *     Upon successful completion of shutdown request, PR_Shutdown
 *     returns PR_SUCCESS.  Otherwise, it returns PR_FAILURE.  Further
 *     failure information can be obtained by calling PR_GetError().
 **************************************************************************
 */

typedef enum PRShutdownHow {
  PR_SHUTDOWN_RCV = 0,  /* disallow further receives */
  PR_SHUTDOWN_SEND = 1, /* disallow further sends */
  PR_SHUTDOWN_BOTH = 2  /* disallow further receives and sends */
} PRShutdownHow;

PRStatus PR_Shutdown(PRFileDesc *fd, PRShutdownHow how);

/*
 *************************************************************************
 * FUNCTION: PR_Recv
 * DESCRIPTION:
 *    Receive a specified number of bytes from a connected socket.
 *     The operation will block until some positive number of bytes are
 *     transferred, a time out has occurred, or there is an error.
 *     No more than 'amount' bytes will be transferred.
 * INPUTS:
 *     PRFileDesc *fd
 *       points to a PRFileDesc object representing a socket.
 *     void *buf
 *       pointer to a buffer to hold the data received.
 *     int32_t amount
 *       the size of 'buf' (in bytes)
 *     int flags
 *       must be zero or PR_MSG_PEEK.
 *     int timeout
 *       Time limit for completion of the receive operation.
 * OUTPUTS:
 *     None
 * RETURN: int32_t
 *         a positive number indicates the number of bytes actually received.
 *         0 means the network connection is closed.
 *         -1 indicates a failure. The reason for the failure is obtained
 *         by calling PR_GetError().
 **************************************************************************
 */

#define PR_MSG_PEEK 0x2

int32_t PR_Recv(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                int timeout);

/*
 *************************************************************************
 * FUNCTION: PR_Send
 * DESCRIPTION:
 *    Send a specified number of bytes from a connected socket.
 *     The operation will block until all bytes are
 *     processed, a time out has occurred, or there is an error.
 * INPUTS:
 *     PRFileDesc *fd
 *       points to a PRFileDesc object representing a socket.
 *     void *buf
 *       pointer to a buffer from where the data is sent.
 *     int32_t amount
 *       the size of 'buf' (in bytes)
 *     int flags
 *        (OBSOLETE - must always be zero)
 *     int timeout
 *       Time limit for completion of the send operation.
 * OUTPUTS:
 *     None
 * RETURN: int32_t
 *     A positive number indicates the number of bytes successfully processed.
 *     This number must always equal 'amount'. A -1 is an indication that the
 *     operation failed. The reason for the failure is obtained by calling
 *     PR_GetError().
 **************************************************************************
 */

int32_t PR_Send(PRFileDesc *fd, const void *buf, int32_t amount, int flags,
                int timeout);

/*
 *************************************************************************
 * FUNCTION: PR_RecvFrom
 * DESCRIPTION:
 *     Receive up to a specified number of bytes from socket which may
 *     or may not be connected.
 *     The operation will block until one or more bytes are
 *     transferred, a time out has occurred, or there is an error.
 *     No more than 'amount' bytes will be transferred.
 * INPUTS:
 *     PRFileDesc *fd
 *       points to a PRFileDesc object representing a socket.
 *     void *buf
 *       pointer to a buffer to hold the data received.
 *     int32_t amount
 *       the size of 'buf' (in bytes)
 *     int flags
 *        (OBSOLETE - must always be zero)
 *     PNetAddr *addr
 *       Specifies the address of the sending peer. It may be NULL.
 *     int timeout
 *       Time limit for completion of the receive operation.
 * OUTPUTS:
 *     None
 * RETURN: int32_t
 *         a positive number indicates the number of bytes actually received.
 *         0 means the network connection is closed.
 *         -1 indicates a failure. The reason for the failure is obtained
 *         by calling PR_GetError().
 **************************************************************************
 */

int32_t PR_RecvFrom(PRFileDesc *fd, void *buf, int32_t amount, int flags,
                    PNetAddr *addr, int timeout);

/*
 *************************************************************************
 * FUNCTION: PR_SendTo
 * DESCRIPTION:
 *    Send a specified number of bytes from an unconnected socket.
 *    The operation will block until all bytes are
 *    sent, a time out has occurred, or there is an error.
 * INPUTS:
 *     PRFileDesc *fd
 *       points to a PRFileDesc object representing an unconnected socket.
 *     void *buf
 *       pointer to a buffer from where the data is sent.
 *     int32_t amount
 *       the size of 'buf' (in bytes)
 *     int flags
 *        (OBSOLETE - must always be zero)
 *     PNetAddr *addr
 *       Specifies the address of the peer.
.*     int timeout
 *       Time limit for completion of the send operation.
 * OUTPUTS:
 *     None
 * RETURN: int32_t
 *     A positive number indicates the number of bytes successfully sent.
 *     -1 indicates a failure. The reason for the failure is obtained
 *     by calling PR_GetError().
 **************************************************************************
 */

int32_t PR_SendTo(PRFileDesc *fd, const void *buf, int32_t amount, int flags,
                  const PNetAddr *addr, int timeout);

/*
*************************************************************************
** FUNCTION: PR_NewTCPSocketPair
** DESCRIPTION:
**    Create a new TCP socket pair. The returned descriptors can be used
**    interchangeably; they are interconnected full-duplex descriptors: data
**    written to one can be read from the other and vice-versa.
**
** INPUTS:
**    None
** OUTPUTS:
**    PRFileDesc *fds[2]
**        The file descriptor pair for the newly created TCP sockets.
** RETURN: PRStatus
**     Upon successful completion of TCP socket pair, PR_NewTCPSocketPair
**     returns PR_SUCCESS.  Otherwise, it returns PR_FAILURE.  Further
**     failure information can be obtained by calling PR_GetError().
** XXX can we implement this on windoze and mac?
**************************************************************************
**/
PRStatus PR_NewTCPSocketPair(PRFileDesc *fds[2]);

/*
*************************************************************************
** FUNCTION: PR_GetSockName
** DESCRIPTION:
**    Get socket name.  Return the network address for this socket.
**
** INPUTS:
**     PRFileDesc *fd
**       Points to a PRFileDesc object representing the socket.
** OUTPUTS:
**     PNetAddr *addr
**       Returns the address of the socket in its own communication space.
** RETURN: PRStatus
**     Upon successful completion, PR_GetSockName returns PR_SUCCESS.
**     Otherwise, it returns PR_FAILURE.  Further failure information can
**     be obtained by calling PR_GetError().
**************************************************************************
**/
PRStatus PR_GetSockName(PRFileDesc *socketFD, PNetAddr *addr);

/*
*************************************************************************
** FUNCTION: PR_GetPeerName
** DESCRIPTION:
**    Get name of the connected peer.  Return the network address for the
**    connected peer socket.
**
** INPUTS:
**     PRFileDesc *fd
**       Points to a PRFileDesc object representing the connected peer.
** OUTPUTS:
**     PNetAddr *addr
**       Returns the address of the connected peer in its own communication
**       space.
** RETURN: PRStatus
**     Upon successful completion, PR_GetPeerName returns PR_SUCCESS.
**     Otherwise, it returns PR_FAILURE.  Further failure information can
**     be obtained by calling PR_GetError().
**************************************************************************
**/
PRStatus PR_GetPeerName(PRFileDesc *socketFD, PNetAddr *addr);

PRStatus PR_GetSocketOption(PRFileDesc *socketFD, PRSocketOptionData *data);

PRStatus PR_SetSocketOption(PRFileDesc *socketFD,
                            const PRSocketOptionData *data);

/*
 **************************************************************************
 * FUNCTION: PR_Close
 * DESCRIPTION:
 *     Close a file or socket.
 * INPUTS:
 *     PRFileDesc *fd
 *         a pointer to a PRFileDesc.
 * OUTPUTS:
 *     None.
 * RETURN:
 *     PRStatus
 * SIDE EFFECTS:
 * RESTRICTIONS:
 *     None.
 * MEMORY:
 *     The dynamic memory pointed to by the argument fd is freed.
 **************************************************************************
 */

PRStatus PR_Close(PRFileDesc *fd);

/*
 **************************************************************************
 * FUNCTION: PR_Read
 * DESCRIPTION:
 *     Read bytes from a file or socket.
 *     The operation will block until either an end of stream indication is
 *     encountered, some positive number of bytes are transferred, or there
 *     is an error. No more than 'amount' bytes will be transferred.
 * INPUTS:
 *     PRFileDesc *fd
 *         pointer to the PRFileDesc object for the file or socket
 *     void *buf
 *         pointer to a buffer to hold the data read in.
 *     int32_t amount
 *         the size of 'buf' (in bytes)
 * OUTPUTS:
 * RETURN:
 *     int32_t
 *         a positive number indicates the number of bytes actually read in.
 *         0 means end of file is reached or the network connection is closed.
 *         -1 indicates a failure. The reason for the failure is obtained
 *         by calling PR_GetError().
 * SIDE EFFECTS:
 *     data is written into the buffer pointed to by 'buf'.
 * RESTRICTIONS:
 *     None.
 * MEMORY:
 *     N/A
 * ALGORITHM:
 *     N/A
 **************************************************************************
 */

int32_t PR_Read(PRFileDesc *fd, void *buf, int32_t amount);

/*
 ***************************************************************************
 * FUNCTION: PR_Write
 * DESCRIPTION:
 *     Write a specified number of bytes to a file or socket.  The thread
 *     invoking this function blocks until all the data is written.
 * INPUTS:
 *     PRFileDesc *fd
 *         pointer to a PRFileDesc object that refers to a file or socket
 *     const void *buf
 *         pointer to the buffer holding the data
 *     int32_t amount
 *         amount of data in bytes to be written from the buffer
 * OUTPUTS:
 *     None.
 * RETURN: int32_t
 *     A positive number indicates the number of bytes successfully written.
 *     A -1 is an indication that the operation failed. The reason
 *     for the failure is obtained by calling PR_GetError().
 ***************************************************************************
 */

int32_t PR_Write(PRFileDesc *fd, const void *buf, int32_t amount);

/************************************************************************/
/************** The following definitions are for poll ******************/
/************************************************************************/

struct PRPollDesc {
  PRFileDesc *fd;
  int16_t in_flags;
  int16_t out_flags;
};

#ifndef _WIN32
#include <poll.h>
#define PR_POLL_READ POLLIN
#define PR_POLL_WRITE POLLOUT
#define PR_POLL_EXCEPT POLLPRI
#define PR_POLL_ERR POLLERR   /* only in out_flags */
#define PR_POLL_NVAL POLLNVAL /* only in out_flags when fd is bad */
#define PR_POLL_HUP POLLHUP   /* only in out_flags */
#else
#define PR_POLL_READ 0x1
#define PR_POLL_WRITE 0x2
#define PR_POLL_EXCEPT 0x4
#define PR_POLL_ERR 0x8   /* only in out_flags */
#define PR_POLL_NVAL 0x10 /* only in out_flags when fd is bad */
#define PR_POLL_HUP 0x20  /* only in out_flags */
#endif

/*
*************************************************************************
** FUNCTION:    PR_Poll
** DESCRIPTION:
**
** The call returns as soon as I/O is ready on one or more of the underlying
** socket objects. A count of the number of ready descriptors is
** returned unless a timeout occurs in which case zero is returned.
**
** PRPollDesc.fd should be set to a pointer to a PRFileDesc object
** representing a socket. This field can be set to NULL to indicate to
** PR_Poll that this PRFileDesc object should be ignored.
** PRPollDesc.in_flags should be set to the desired request
** (read/write/except or some combination). Upon successful return from
** this call PRPollDesc.out_flags will be set to indicate what kind of
** i/o can be performed on the respective descriptor. PR_Poll() uses the
** out_flags fields as scratch variables during the call. If PR_Poll()
** returns 0 or -1, the out_flags fields do not contain meaningful values
** and must not be used.
**
** INPUTS:
**      PRPollDesc *pds         A pointer to an array of PRPollDesc
**
**      int npds             The number of elements in the array
**                              If this argument is zero PR_Poll is
**                              equivalent to a PR_Sleep(timeout).
**
**      int timeout  Amount of time the call will block waiting
**                              for I/O to become ready. If this time expires
**                              w/o any I/O becoming ready, the result will
**                              be zero.
**
** OUTPUTS:    None
** RETURN:
**      int32_t                 Number of PRPollDesc's with events or zero
**                              if the function timed out or -1 on failure.
**                              The reason for the failure is obtained by
**                              calling PR_GetError().
**************************************************************************
*/
int32_t PR_Poll(PRPollDesc *pds, int npds, int timeout);

/* prnetdb.h */
/***********************************************************************
** FUNCTION: PR_InitializeNetAddr(),
** DESCRIPTION:
**  Initialize the fields of a PNetAddr, assigning well known values as
**  appropriate.
**
** INPUTS
**  PNetAddrValue val  The value to be assigned to the IP Address portion
**                      of the network address. This can only specify the
**                      special well known values that are equivalent to
**                      INADDR_ANY and INADDR_LOOPBACK.
**
**  uint16_t port       The port number to be assigned in the structure.
**
** OUTPUTS:
**  PNetAddr *addr     The address to be manipulated.
**
** RETURN:
**  PRStatus            To indicate success or failure. If the latter, the
**                      reason for the failure can be retrieved by calling
**                      PR_GetError();
***********************************************************************/
typedef enum PNetAddrValue {
  P_IpAddrNull,     /* do NOT overwrite the IP address */
  P_IpAddrAny,      /* assign logical INADDR_ANY to IP address */
  P_IpAddrLoopback, /* assign logical INADDR_LOOPBACK  */
  P_IpAddrV4Mapped  /* IPv4 mapped address */
} PNetAddrValue;

PRStatus PR_InitializeNetAddr(PNetAddrValue val, uint16_t port, PNetAddr *addr);

/***********************************************************************
** FUNCTION: PR_SetNetAddr(),
** DESCRIPTION:
**  Set the fields of a PRNetAddr, assigning well known values as
**  appropriate. This function is similar to PR_InitializeNetAddr
**  but differs in that the address family is specified.
**
** INPUTS
**  PRNetAddrValue val  The value to be assigned to the IP Address portion
**                      of the network address. This can only specify the
**                      special well known values that are equivalent to
**                      INADDR_ANY and INADDR_LOOPBACK.
**
**  PRUint16 af         The address family (either PR_AF_INET or PR_AF_INET6)
**
**  PRUint16 port       The port number to be assigned in the structure.
**
** OUTPUTS:
**  PRNetAddr *addr     The address to be manipulated.
**
** RETURN:
**  PRStatus            To indicate success or failure. If the latter, the
**                      reason for the failure can be retrieved by calling
**                      PR_GetError();
***********************************************************************/
PRStatus PR_SetNetAddr(PNetAddrValue val, uint16_t af, uint16_t port,
                       PNetAddr *addr);

/*
 *********************************************************************
 *  Translate an Internet address to/from a character string
 *********************************************************************
 */
PRStatus PR_StringToNetAddr(const char *string, PNetAddr *addr);

PRStatus PR_NetAddrToString(const PNetAddr *addr, char *string, uint32_t size);

/* primpl.h */
extern bool _pr_initialized;
extern void _PR_ImplicitInitialization();

/* prthread.h */
struct PRThread {
  /*
  ** Per thread private data
  */
  uint32_t tpdLength;      /* thread's current vector length */
  void **privateData;      /* private data vector or NULL */
  PRErrorCode errorCode;   /* current NSPR error code | zero */
  int32_t osErrorCode;     /* mapping of errorCode | zero */
  int errorStringLength;   /* textLength from last call to PR_SetErrorText() */
  int32_t errorStringSize; /* malloc()'d size of buffer | zero */
  char *errorString;       /* current error string | NULL */
  char *name;              /* thread's name */
};

#endif // PR_UTIL
