// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2020 Chilledheart  */

#ifndef PSOCKET_TRANSPORT
#define PSOCKET_TRANSPORT

#include "stream.hpp"
#include <string>

using InputOutputStream = stream;

class PSocketTransport {
public:
  /**
   * Returns the IP address of the socket connection peer.
   */
  const PNetAddr &getPeerAddr() const { return peer_addr_; }

  /**
   * Returns the IP address of the initiating end.
   */
  const PNetAddr &getSelfAddr() const { return self_addr_; }

  /**
   * Bind to a specific local address.
   */
  bool bind(const PNetAddr &);

  /**
   * Test if this socket transport is (still) connected.
   */
  bool isAlive();

  /**
   * Socket timeouts in seconds.  To specify no timeout, pass UINT32_MAX
   * as aValue to setTimeout.  The implementation may truncate timeout values
   * to a smaller range of values (e.g., 0 to 0xFFFF).
   */
  unsigned long getTimeout(in unsigned long aType) const;
  void setTimeout(in unsigned long aType, in unsigned long aValue);

  /**
   * True to set addr and port reuse socket options.
   */
  void setReuseAddrPort(in bool reuseAddrPort) const;

  /**
   * TCP send and receive buffer sizes. A value of 0 means OS level
   * auto-tuning is in effect.
   */
  unsigned long getRecvBufferSize() const;
  unsigned long getSendBufferSize() const;

  /**
   * TCP keepalive configuration (support varies by platform).
   * Note that the attribute as well as the setter can only accessed
   * in the socket thread.
   */
  bool getKeepaliveEnabled() const;
  void setKeepaliveVals(in long keepaliveIdleTime,
                        in long keepaliveRetryInterval);

  bool OpenInputStream(uint32_t flags, uint32_t segsize, uint32_t segcount,
                       InputOutputStream **result)

      bool OpenOutputStream(uint32_t flags, uint32_t segsize, uint32_t segcount,
                            InputOutputStream **result)

      // default construct of PSocketTransport
      PSocketTransport();

  // this method instructs the socket transport to open a socket of the
  // given type(s) to the given host.
  bool Init(const char **socketTypes, uint32_t typeCount,
            const std::string &host, uint16_t port,
            const std::string &hostRoute, uint16_t portRoute);

  // this method instructs the socket transport to use an already connected
  // socket with the given address.
  bool InitWithConnectedSocket(PRFileDesc *socketFD, const PNetAddr *addr);

  //
  // called to service a socket
  //
  // params:
  //   socketRef - socket identifier
  //   fd        - socket file descriptor
  //   outFlags  - value of PR_PollDesc::out_flags after PR_Poll returns
  //               or -1 if a timeout occurred
  //
  void OnSocketReady(PRFileDesc *, int16_t outFlags);

  //
  // called when a socket is no longer under the control of the socket
  // transport service.  the socket handler may close the socket at this
  // point.  after this call returns, the handler will no longer be owned
  // by the socket transport service.
  //
  void OnSocketDetached(PRFileDesc *);

  //
  // called to determine if the socket is for a local peer.
  // when used for server sockets, indicates if it only accepts local
  // connections.
  //
  void IsLocal(bool *aIsLocal);

  //
  // called to determine if this socket should not be terminated when Gecko
  // is turned offline. This is mostly useful for the debugging server
  // socket.
  //
  void KeepWhenOffline(bool *aKeepWhenOffline);

  //
  // called when global pref for keepalive has changed.
  //
  void OnKeepaliveEnabledPrefChange(bool aEnabled);

  //
  // returns the number of bytes sent/transmitted over the socket
  //
  uint64_t ByteCountReceived() { return rByteCount_; }
  uint64_t ByteCountSent() { return wByteCount_; }

private:
  // called when the socket is connected
  void OnSocketConnected();

  // socket methods
  void SendStatus(bool status);
  bool ResolveHost();
  bool BuildSocket(PRFileDesc *&, bool &, bool &);
  bool InitiateSocket();
  bool RecoverFromError();

private:
  std::string host_;
  uint16_t port_;

  // self_addr_/peer_addr_ is valid from GetPeerAddr()/GetSelfAddr() once we
  // have reached STATE_TRANSFERRING. It must not change after that.
  void SetSocketName(PRFileDesc *fd);
  PRFileDesc fd_;
  PNetAddr peer_addr_;
  PNetAddr self_addr_;

  InputOutputStream yStream_;

  uint64_t rByteCount_;
  uint64_t wByteCount_;
};
#endif // PSOCKET_TRANSPORT
