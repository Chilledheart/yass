// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "net/socket_bio_adapter.hpp"
#include "net/openssl_util.hpp"

namespace net {

SocketBIOAdapter::SocketBIOAdapter(asio::io_context *io_context,
                                   asio::ip::tcp::socket* socket,
                                   int read_buffer_capacity,
                                   int write_buffer_capacity,
                                   Delegate* delegate)
    : io_context_(io_context),
      socket_(socket),
      read_buffer_capacity_(read_buffer_capacity),
      write_buffer_capacity_(write_buffer_capacity),
      delegate_(delegate) {
  bio_.reset(BIO_new(&kBIOMethod));
  bio_->ptr = this;
  bio_->init = 1;

  asio::error_code ec;
  socket_->native_non_blocking(true, ec);
  socket_->non_blocking(true, ec);

  read_callback_ = [this](int result) { OnSocketReadComplete(result); };
  write_callback_ = [this](int result) { OnSocketWriteComplete(result); };
}

SocketBIOAdapter::~SocketBIOAdapter() {
  // BIOs are reference-counted and may outlive the adapter. Clear the pointer
  // so future operations fail.
  bio_->ptr = nullptr;
}

bool SocketBIOAdapter::HasPendingReadData() {
  return read_result_ > 0;
}

size_t SocketBIOAdapter::GetAllocationSize() const {
  size_t buffer_size = 0;
  if (read_buffer_)
    buffer_size += read_buffer_capacity_;

  if (write_buffer_)
    buffer_size += write_buffer_capacity_;
  return buffer_size;
}

int SocketBIOAdapter::BIORead(char* out, int len) {
  if (len <= 0)
    return len;

  // If there is no result available synchronously, report any Write() errors
  // that were observed. Otherwise the application may have encountered a socket
  // error while writing that would otherwise not be reported until the
  // application attempted to write again - which it may never do. See
  // https://crbug.com/249848.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING &&
      (read_result_ == 0 || read_result_ == ERR_IO_PENDING)) {
    OpenSSLPutNetError(FROM_HERE, write_error_);
    return -1;
  }

  if (read_result_ == 0) {
    // Instantiate the read buffer and read from the socket. Although only |len|
    // bytes were requested, intentionally read to the full buffer size. The SSL
    // layer reads the record header and body in separate reads to avoid
    // overreading, but issuing one is more efficient. SSL sockets are not
    // reused after shutdown for non-SSL traffic, so overreading is fine.
    DCHECK(!read_buffer_);
    DCHECK_EQ(0, read_offset_);
    read_buffer_ = IOBuf::create(read_buffer_capacity_);
    asio::error_code ec;
    size_t result = socket_->read_some(mutable_buffer(*read_buffer_), ec);
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      OnBIORead();
    }
    if (!ec) {
      read_buffer_->append(result);
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      read_result_ = ERR_IO_PENDING;
    } else {
      HandleSocketReadResult(ec == asio::error::eof ? 0 : (ec ? ERR_UNEXPECTED : (int)result));
    }
  }

  // There is a pending Read(). Inform the caller to retry when it completes.
  if (read_result_ == ERR_IO_PENDING) {
    BIO_set_retry_read(bio());
    return -1;
  }

  // If the last Read() failed, report the error.
  if (read_result_ < 0) {
    OpenSSLPutNetError(FROM_HERE, read_result_);
    return -1;
  }

  // Report the result of the last Read() if non-empty.
  CHECK_LT(read_offset_, read_result_);
  len = std::min(len, read_result_ - read_offset_);
  memcpy(out, read_buffer_->data() + read_offset_, len);
  read_offset_ += len;

  // Release the buffer when empty.
  if (read_offset_ == read_result_) {
    read_buffer_ = nullptr;
    read_offset_ = 0;
    read_result_ = 0;
  }

  return len;
}

void SocketBIOAdapter::OnBIORead() {
  socket_->async_wait(asio::ip::tcp::socket::wait_read,
    [this](asio::error_code ec) {
    if (ec) {
      read_callback_.operator()(ERR_UNEXPECTED);
      return;
    }
    size_t bytes_transferred;
    do {
      ec = asio::error_code();
      bytes_transferred = socket_->read_some(mutable_buffer(*read_buffer_), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      OnBIORead();
      return;
    }
    int result;
    if (ec == asio::error::eof) {
      result = 0;
    } else if (ec) {
      result = ERR_UNEXPECTED;
    } else {
      result = bytes_transferred;
      read_buffer_->append(bytes_transferred);
    }
    read_callback_.operator()(result);
  });
}

void SocketBIOAdapter::HandleSocketReadResult(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  // If an EOF, canonicalize to ERR_CONNECTION_CLOSED here, so that higher
  // levels don't report success.
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  read_result_ = result;

  // The read buffer is no longer needed.
  if (read_result_ <= 0)
    read_buffer_ = nullptr;
}

void SocketBIOAdapter::OnSocketReadComplete(int result) {
  DCHECK_EQ(ERR_IO_PENDING, read_result_);

  HandleSocketReadResult(result);
  delegate_->OnReadReady();
}

void SocketBIOAdapter::OnSocketReadIfReadyComplete(int result) {
  DCHECK_EQ(ERR_IO_PENDING, read_result_);
  DCHECK_GE(OK, result);

  // Do not use HandleSocketReadResult() because result == OK doesn't mean EOF.
  read_result_ = result;

  delegate_->OnReadReady();
}

int SocketBIOAdapter::BIOWrite(const char* in, int len) {
  if (len <= 0)
    return len;

  // If the write buffer is not empty, there must be a pending Write() to flush
  // it.
  DCHECK(write_buffer_used_ == 0 || write_error_ == ERR_IO_PENDING);

  // If a previous Write() failed, report the error.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING) {
    OpenSSLPutNetError(FROM_HERE, write_error_);
    return -1;
  }

  // Instantiate the write buffer if needed.
  if (!write_buffer_) {
    DCHECK_EQ(0, write_buffer_used_);
    write_buffer_ = IOBuf::create(write_buffer_capacity_);
  }

  // If the ring buffer is full, inform the caller to try again later.
  if (write_buffer_used_ == (int)write_buffer_->capacity()) {
    BIO_set_retry_write(bio());
    return -1;
  }

  int bytes_copied = 0;

  // If there is space after the offset, fill it.
  if (write_buffer_used_ < (int)write_buffer_->tailroom()) {
    int chunk = std::min<int>(write_buffer_->tailroom() - write_buffer_used_, len);
    memcpy(write_buffer_->mutable_tail() + write_buffer_used_, in, chunk);
    in += chunk;
    len -= chunk;
    bytes_copied += chunk;
    write_buffer_used_ += chunk;
  }

  // If there is still space for remaining data, try to wrap around.
  if (len > 0 && write_buffer_used_ < (int)write_buffer_->capacity()) {
    // If there were any room after the offset, the previous branch would have
    // filled it.
    CHECK_LE((int)write_buffer_->tailroom(), write_buffer_used_);
    int write_offset = write_buffer_used_ - write_buffer_->tailroom();
    int chunk = std::min<int>(len, write_buffer_->capacity() - write_buffer_used_);
    memcpy(write_buffer_->mutable_buffer() + write_offset, in, chunk);
    in += chunk;
    len -= chunk;
    bytes_copied += chunk;
    write_buffer_used_ += chunk;
  }

  // Either the buffer is now full or there is no more input.
  DCHECK(len == 0 || write_buffer_used_ == (int)write_buffer_->capacity());

  // Schedule a socket Write() if necessary. (The ring buffer may previously
  // have been empty.)
  SocketWrite();

  // If a read-interrupting write error was synchronously discovered,
  // asynchronously notify OnReadReady. See https://crbug.com/249848. Avoid
  // reentrancy by deferring it to a later event loop iteration.
  if (write_error_ != OK && write_error_ != ERR_IO_PENDING &&
      read_result_ == ERR_IO_PENDING) {
    io_context_->post([this]() {
      CallOnReadReady();
    });
  }

  return bytes_copied;
}

void SocketBIOAdapter::SocketWrite() {
  while (write_error_ == OK && write_buffer_used_ > 0) {
    int write_size = std::min<int>(write_buffer_used_, write_buffer_->tailroom());
    asio::error_code ec;
    size_t result;
    do {
      ec = asio::error_code();
      result = socket_->write_some(asio::const_buffer(write_buffer_->tail(), write_size), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    if ((int)result != write_size || ec == asio::error::try_again || ec == asio::error::would_block) {
      asio::async_write(*socket_,
                        asio::const_buffer(write_buffer_->tail() + result, write_size - result),
                        [this](asio::error_code ec, size_t bytes_transferred) {
        if (ec) {
          write_callback_(ERR_UNEXPECTED);
          return;
        }
        write_callback_(bytes_transferred);
      });
    }
    if ((int)result != write_size || ec == asio::error::try_again || ec == asio::error::would_block) {
      write_error_ = ERR_IO_PENDING;
      return;
    }

    HandleSocketWriteResult(ec ? ERR_UNEXPECTED : (int)result);
  }
}

void SocketBIOAdapter::HandleSocketWriteResult(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result < 0) {
    write_error_ = result;

    // The write buffer is no longer needed.
    write_buffer_ = nullptr;
    write_buffer_used_ = 0;
    return;
  }

  // Advance the ring buffer.
  write_buffer_->append(result);
  write_buffer_used_ -= result;
  if (write_buffer_->tailroom() == 0)
    write_buffer_->clear();
  write_error_ = OK;

  // Release the write buffer if empty.
  if (write_buffer_used_ == 0)
    write_buffer_ = nullptr;
}

void SocketBIOAdapter::OnSocketWriteComplete(int result) {
  DCHECK_EQ(ERR_IO_PENDING, write_error_);

  bool was_full = write_buffer_used_ == (int)write_buffer_->capacity();

  HandleSocketWriteResult(result);
  SocketWrite();

  // If transitioning from being unable to accept data to being able to, signal
  // OnWriteReady.
  if (was_full) {
#if 0
    base::WeakPtr<SocketBIOAdapter> guard(weak_factory_.GetWeakPtr());
#endif
    delegate_->OnWriteReady();
    // OnWriteReady may delete the adapter.
#if 0
    if (!guard)
      return;
#endif
  }

  // Write errors are fed back into BIO_read once the read buffer is empty. If
  // BIO_read is currently blocked, signal early that a read result is ready.
  if (result < 0 && read_result_ == ERR_IO_PENDING)
    delegate_->OnReadReady();
}

void SocketBIOAdapter::CallOnReadReady() {
  if (read_result_ == ERR_IO_PENDING)
    delegate_->OnReadReady();
}

SocketBIOAdapter* SocketBIOAdapter::GetAdapter(BIO* bio) {
  DCHECK_EQ(&kBIOMethod, bio->method);
  SocketBIOAdapter* adapter = reinterpret_cast<SocketBIOAdapter*>(bio->ptr);
  if (adapter)
    DCHECK_EQ(bio, adapter->bio());
  return adapter;
}

int SocketBIOAdapter::BIOWriteWrapper(BIO* bio, const char* in, int len) {
  BIO_clear_retry_flags(bio);

  SocketBIOAdapter* adapter = GetAdapter(bio);
  if (!adapter) {
    OpenSSLPutNetError(FROM_HERE, ERR_UNEXPECTED);
    return -1;
  }

  return adapter->BIOWrite(in, len);
}

int SocketBIOAdapter::BIOReadWrapper(BIO* bio, char* out, int len) {
  BIO_clear_retry_flags(bio);

  SocketBIOAdapter* adapter = GetAdapter(bio);
  if (!adapter) {
    OpenSSLPutNetError(FROM_HERE, ERR_UNEXPECTED);
    return -1;
  }

  return adapter->BIORead(out, len);
}

long SocketBIOAdapter::BIOCtrlWrapper(BIO* bio,
                                      int cmd,
                                      long larg,
                                      void* parg) {
  switch (cmd) {
    case BIO_CTRL_FLUSH:
      // The SSL stack requires BIOs handle BIO_flush.
      return 1;
  }

  NOTIMPLEMENTED();
  return 0;
}

const BIO_METHOD SocketBIOAdapter::kBIOMethod = {
    0,        // type (unused)
    nullptr,  // name (unused)
    SocketBIOAdapter::BIOWriteWrapper,
    SocketBIOAdapter::BIOReadWrapper,
    nullptr,  // puts
    nullptr,  // gets
    SocketBIOAdapter::BIOCtrlWrapper,
    nullptr,  // create
    nullptr,  // destroy
    nullptr,  // callback_ctrl
};

}  // namespace net
