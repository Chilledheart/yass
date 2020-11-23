//
// stream_socket.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_STREAM_SOCKET
#define H_STREAM_SOCKET

#include "core/completion_once_callback.hpp"
#include "core/iobuf.hpp"
#include "core/pr_util.hpp"
#include <functional>

class IPEndPoint;
class StreamSocket {
public:
  using BeforeConnectCallback = std::function<int()>;

  virtual ~StreamSocket() {}

  virtual int Read(IOBuf *buf, int buf_len,
                   CompletionOnceCallback callback) = 0;

  virtual int ReadIfReady(IOBuf *buf, int buf_len,
                          CompletionOnceCallback callback);

  virtual int CancelReadIfReady();

  virtual int Write(IOBuf *buf, int buf_len,
                    CompletionOnceCallback callback) = 0;

  virtual int SetReceiveBufferSize(int32_t size) = 0;
  virtual int SetSendBufferSize(int32_t size) = 0;

  virtual void SetBeforeConnectCallback(
      const BeforeConnectCallback &before_connect_callback);

  virtual int Connect(CompletionOnceCallback callback) = 0;
  virtual void Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual int GetPeerAddress(IPEndPoint *address) const = 0;
  virtual int GetLocalAddress(IPEndPoint *address) const = 0;
};

#endif // H_STREAM_SOCKET