// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2019-2023 Chilledheart  */

#include "ss_connection.hpp"

#include <absl/base/attributes.h>

#include "config/config.hpp"
#include "core/asio.hpp"
#include "core/cipher.hpp"
#include "core/utils.hpp"

namespace {
// from spdy_session.h
// If more than this many bytes have been read or more than that many
// milliseconds have passed, return ERR_IO_PENDING from ReadLoop.
const int kYieldAfterBytesRead = 32 * 1024;
const int kYieldAfterDurationMilliseconds = 20;
} // namespace

// 32K / 4k = 8
#define MAX_DOWNSTREAM_DEPS 8
#define MAX_UPSTREAM_DEPS 8

namespace ss {

SsConnection::SsConnection(asio::io_context& io_context,
                           const asio::ip::tcp::endpoint& remote_endpoint)
    : Connection(io_context, remote_endpoint),
      state_(),
      resolver_(*io_context_),
      encoder_(new cipher(
          "",
          absl::GetFlag(FLAGS_password),
          static_cast<enum cipher_method>(absl::GetFlag(FLAGS_cipher_method)),
          true)),
      decoder_(new cipher("",
                          absl::GetFlag(FLAGS_password),
                          static_cast<enum cipher_method>(
                              absl::GetFlag(FLAGS_cipher_method)))) {}

SsConnection::~SsConnection() {
  VLOG(2) << "Connection (server) " << connection_id() << " freed memory";
}

void SsConnection::start() {
  SetState(state_handshake);
  closed_ = false;
  upstream_writable_ = false;
  downstream_readable_ = true;
  asio::error_code ec;
  socket_.native_non_blocking(true, ec);
  socket_.non_blocking(true, ec);
  ReadHandshake();
}

void SsConnection::close() {
  if (closed_) {
    return;
  }
  size_t bytes = 0;
  for (auto buf : downstream_)
    bytes += buf->length();
  VLOG(2) << "Connection (server) " << connection_id()
          << " disconnected with client at stage: "
          << SsConnection::state_to_str(CurrentState())
          << " and remaining: " << bytes << " bytes.";
  asio::error_code ec;
  closed_ = true;
  socket_.close(ec);
  if (ec) {
    VLOG(2) << "close() error: " << ec;
  }
  if (channel_) {
    channel_->close();
  }
  resolver_.cancel();
  auto cb = std::move(disconnect_cb_);
  if (cb) {
    cb();
  }
}

void SsConnection::ReadHandshake() {
  scoped_refptr<SsConnection> self(this);

  socket_.async_read_some(asio::null_buffers(),
      [self](asio::error_code ec, size_t bytes_transferred) {
        std::shared_ptr<IOBuf> cipherbuf{IOBuf::create(SOCKET_BUF_SIZE).release()};
        cipherbuf->reserve(0, SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->socket_.read_some(mutable_buffer(*cipherbuf), ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadHandshake();
          return;
        }
        if (ec) {
          self->OnDisconnect(ec);
          return;
        }
        cipherbuf->append(bytes_transferred);
        std::shared_ptr<IOBuf> buf = self->DecryptData(cipherbuf);

        DumpHex("HANDSHAKE->", buf.get());

        request_parser::result_type result;
        std::tie(result, std::ignore) = self->request_parser_.parse(
            self->request_, buf->data(), buf->data() + bytes_transferred);

        if (result == request_parser::good) {
          buf->trimStart(self->request_.length());
          buf->retreat(self->request_.length());
          DCHECK_LE(self->request_.length(), bytes_transferred);
          self->ProcessReceivedData(buf, ec, buf->length());
        } else {
          // FIXME better error code?
          ec = asio::error::connection_refused;
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ResolveDns(std::shared_ptr<IOBuf> buf) {
  scoped_refptr<SsConnection> self(this);
  resolver_.async_resolve(
      self->request_.domain_name(), std::to_string(self->request_.port()),
      [self, buf](asio::error_code ec,
                  asio::ip::tcp::resolver::results_type results) {
        // Get a list of endpoints corresponding to the SOCKS 5 domain name.
        if (!ec) {
          self->remote_endpoint_ = results->endpoint();
          VLOG(3) << "Connection (server) " << self->connection_id()
                  << " resolved address: " << self->request_.domain_name()
                  << " to: " << self->remote_endpoint_;
          self->SetState(state_stream);
          self->OnConnect();
          self->ProcessReceivedData(buf, ec, buf->length());
        } else {
          self->OnDisconnect(ec);
        }
      });
}

void SsConnection::ReadStream() {
  scoped_refptr<SsConnection> self(this);
  downstream_read_inprogress_ = true;

  socket_.async_read_some(asio::null_buffers(),
      [self](asio::error_code ec,
             std::size_t bytes_transferred) {
        self->downstream_read_inprogress_ = false;
        if (ec) {
          self->ProcessReceivedData(nullptr, ec, bytes_transferred);
          return;
        }
        if (!self->downstream_readable_) {
          return;
        }
        std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
        buf->reserve(0, SOCKET_BUF_SIZE);
        if (!ec) {
          do {
            bytes_transferred = self->socket_.read_some(mutable_buffer(*buf), ec);
            if (ec == asio::error::interrupted) {
              continue;
            }
          } while(false);
        }
        if (ec == asio::error::try_again || ec == asio::error::would_block) {
          self->ReadStream();
          return;
        }
        buf->append(bytes_transferred);
        auto plainbuf = self->DecryptData(buf);
        if (!plainbuf->empty()) {
          self->ProcessReceivedData(plainbuf, ec, plainbuf->length());
        }
      });
}

void SsConnection::WriteStream() {
  scoped_refptr<SsConnection> self(this);
  socket_.async_write_some(asio::null_buffers(),
      [self](asio::error_code ec, size_t /*bytes_transferred*/) {
        if (ec) {
          self->ProcessSentData(ec, 0);
          return;
        }
        self->WriteStreamInPipe();
      });
}

void SsConnection::WriteStreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0;
  uint64_t next_ticks = GetMonotonicTime() +
    kYieldAfterDurationMilliseconds * 1000 * 1000;

  /* recursively send the remainings */
  while (!closed_) {
    if (GetMonotonicTime() > next_ticks) {
      break;
    }
    if (bytes_transferred > kYieldAfterBytesRead) {
      break;
    }

    bool eof = false;
    auto buf = GetNextDownstreamBuf(ec);
    size_t read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      eof = true;
    } else if (ec) {
      /* safe to return, channel will handle this error */
      ec = asio::error_code();
      break;
    }
    if (!read) {
      break;
    }
    size_t written;
    do {
      ec = asio::error_code();
      written = socket_.write_some(const_buffer(*buf), ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->trimStart(written);
    bytes_transferred += written;
    // continue to resume
    if (buf->empty()) {
      downstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      ec = asio::error_code();
      break;
    }
    if (ec) {
      break;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }
  ProcessSentData(ec, bytes_transferred);
}

std::shared_ptr<IOBuf> SsConnection::GetNextDownstreamBuf(asio::error_code &ec) {
  if (!downstream_.empty()) {
    ec = asio::error_code();
    return downstream_.front();
  }
  if (!upstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);
  size_t read;
  do {
    ec = asio::error_code();
    read = channel_->read_some(buf, ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (read) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: received reply (pipe): " << read << " bytes.";
  } else {
    return nullptr;
  }
  downstream_.push_back(EncryptData(buf));
  return downstream_.front();
}

void SsConnection::WriteUpstreamInPipe() {
  asio::error_code ec;
  size_t bytes_transferred = 0U;
  uint64_t next_ticks = GetMonotonicTime() +
    kYieldAfterDurationMilliseconds * 1000 * 1000;

  /* recursively send the remainings */
  while (!channel_->eof()) {
    if (bytes_transferred > kYieldAfterBytesRead) {
      break;
    }
    if (GetMonotonicTime() > next_ticks) {
      break;
    }
    bool eof = false;
    size_t read;
    std::shared_ptr<IOBuf> buf = GetNextUpstreamBuf(ec);
    read = buf ? buf->length() : 0;
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      eof = true;
    } else if (ec) {
      /* safe to return, channel will handle this error */
      return;
    }
    if (!read) {
      break;
    }
    ec = asio::error_code();
    size_t written;
    do {
      written = channel_->write_some(buf, ec);
      if (ec == asio::error::interrupted) {
        continue;
      }
    } while(false);
    buf->trimStart(written);
    bytes_transferred += written;
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: sent request (pipe): " << written << " bytes"
            << " ec: " << ec << " and data to write: "
            << buf->length();
    // continue to resume
    if (buf->empty()) {
      upstream_.pop_front();
    }
    if (ec == asio::error::try_again || ec == asio::error::would_block) {
      break;
    }
    if (ec) {
      OnDisconnect(ec);
      return;
    }
    if (eof || !buf->empty()) {
      break;
    }
  }
}

std::shared_ptr<IOBuf> SsConnection::GetNextUpstreamBuf(asio::error_code &ec) {
  if (!upstream_.empty()) {
    ec = asio::error_code();
    return upstream_.front();
  }
  if (!downstream_readable_) {
    ec = asio::error::try_again;
    return nullptr;
  }
  std::shared_ptr<IOBuf> buf{IOBuf::create(SOCKET_BUF_SIZE).release()};
  buf->reserve(0, SOCKET_BUF_SIZE);
  size_t read;
  do {
    read = socket_.read_some(mutable_buffer(*buf), ec);
    if (ec == asio::error::interrupted) {
      continue;
    }
  } while(false);
  buf->append(read);
  if (ec && ec != asio::error::try_again && ec != asio::error::would_block) {
    /* safe to return, socket will handle this error later */
    ProcessReceivedData(nullptr, ec, read);
    return nullptr;
  }
  if (read) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " received data (pipe): " << read << " bytes.";
  } else {
    return nullptr;
  }
  rbytes_transferred_ += read;
  auto plainbuf = DecryptData(buf);
  if (!plainbuf->empty()) {
    upstream_.push_back(plainbuf);
  }
  if (upstream_.empty()) {
    ec = asio::error::try_again;
    return nullptr;
  }
  return upstream_.front();
}

void SsConnection::ProcessReceivedData(std::shared_ptr<IOBuf> buf,
                                       asio::error_code ec,
                                       size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " received data: " << bytes_transferred << " bytes"
          << " ec: " << ec;

  rbytes_transferred_ += bytes_transferred;

  if (buf) {
    DCHECK_LE(bytes_transferred, buf->length());
  }

  if (!ec) {
    switch (CurrentState()) {
      case state_handshake:
        if (request_.address_type() == domain) {
          ResolveDns(buf);
          return;
        }
        remote_endpoint_ = request_.endpoint();
        SetState(state_stream);
        OnConnect();
        DCHECK_EQ(buf->length(), bytes_transferred);
        ABSL_FALLTHROUGH_INTENDED;
        /* fall through */
      case state_stream:
        if (bytes_transferred) {
          OnStreamRead(buf);
        }
        if (downstream_readable_) {
          ReadStream();  // continously read
        }
        break;
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    };
  }
#if 1
  // Silence Read EOF error triggered by upstream disconnection
  if (ec == asio::error::eof && channel_ && channel_->eof()) {
    return;
  }
#endif
  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
};

void SsConnection::ProcessSentData(asio::error_code ec,
                                   size_t bytes_transferred) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " sent data: " << bytes_transferred << " bytes"
          << " ec: " << ec << " and data to write: " << downstream_.size();

  wbytes_transferred_ += bytes_transferred;

  if (!ec) {
    switch (CurrentState()) {
      case state_stream:
        OnStreamWrite();
        break;
      case state_handshake:
      case state_error:
        ec = std::make_error_code(std::errc::bad_message);
        break;
      default:
        LOG(FATAL) << "Connection (client) " << connection_id()
                   << " bad state 0x" << std::hex
                   << static_cast<int>(CurrentState()) << std::dec;
    }
  }

  if (ec) {
    SetState(state_error);
    OnDisconnect(ec);
  }
};

void SsConnection::OnConnect() {
  LOG(INFO) << "Connection (server) " << connection_id()
            << " to " << remote_domain();
  channel_ = std::make_unique<stream>(*io_context_, remote_endpoint_, this);
  channel_->connect();
}

void SsConnection::OnStreamRead(std::shared_ptr<IOBuf> buf) {
  // queue limit to downstream read
  if (upstream_.size() >= MAX_UPSTREAM_DEPS && downstream_readable_) {
    DisableStreamRead();
  }

  OnUpstreamWrite(buf);
}

void SsConnection::OnStreamWrite() {
  OnDownstreamWriteFlush();

  /* shutdown the socket if upstream is eof and all remaining data sent */
  if (channel_->eof() && downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " last data sent: shutting down";
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    return;
  }

  /* disable queue limit to re-enable upstream read */
  if (channel_->connected() && downstream_.size() < MAX_DOWNSTREAM_DEPS && !upstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " re-enabling reading from upstream";
    upstream_readable_ = true;
    scoped_refptr<SsConnection> self(this);
    channel_->enable_read([self]() {});
  }
}

void SsConnection::EnableStreamRead() {
  if (!downstream_readable_) {
    downstream_readable_ = true;
    if (!downstream_read_inprogress_) {
      ReadStream();
    }
  }
}

void SsConnection::DisableStreamRead() {
  downstream_readable_ = false;
}

void SsConnection::OnDisconnect(asio::error_code ec) {
  size_t bytes = 0;
  for (auto buf : downstream_)
    bytes += buf->length();
#ifdef WIN32
  if (ec.value() == WSAESHUTDOWN) {
    ec = asio::error_code();
  }
#endif
  LOG(INFO) << "Connection (server) " << connection_id()
            << " closed: " << ec << " remaining: " << bytes << " bytes";
  close();
}

void SsConnection::OnDownstreamWriteFlush() {
  if (!downstream_.empty()) {
    OnDownstreamWrite(nullptr);
  }
}

void SsConnection::OnDownstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    downstream_.push_back(buf);
  }

  if (!downstream_.empty()) {
    WriteStream();
  }
}

void SsConnection::OnUpstreamWriteFlush() {
  OnUpstreamWrite(nullptr);
}

void SsConnection::OnUpstreamWrite(std::shared_ptr<IOBuf> buf) {
  if (buf && !buf->empty()) {
    upstream_.push_back(buf);
  }
  if (!upstream_.empty() && upstream_writable_) {
    upstream_writable_ = false;
    scoped_refptr<SsConnection> self(this);
    channel_->start_write(upstream_.front(), [self](){});
  }
}

void SsConnection::connected() {
  VLOG(2) << "Connection (server) " << connection_id()
          << " remote: established upstream connection with: "
          << remote_domain();
  scoped_refptr<SsConnection> self(this);
  upstream_readable_ = true;
  upstream_writable_ = true;
  channel_->start_read([self](){});
  OnUpstreamWriteFlush();
}

void SsConnection::received(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: received reply: " << buf->length() << " bytes.";

  // queue limit to upstream read
  if (downstream_.size() >= MAX_DOWNSTREAM_DEPS && upstream_readable_) {
    VLOG(2) << "Connection (server) " << connection_id()
            << " disabling reading from upstream";
    upstream_readable_ = false;
    channel_->disable_read();
  }

  OnDownstreamWrite(EncryptData(buf));
}

void SsConnection::sent(std::shared_ptr<IOBuf> buf) {
  VLOG(3) << "Connection (server) " << connection_id()
          << " upstream: sent request: " << buf->length() << " bytes.";
  DCHECK(!upstream_.empty() && upstream_[0] == buf);
  upstream_.pop_front();

  upstream_writable_ = true;

  WriteUpstreamInPipe();
  OnUpstreamWriteFlush();

  if (upstream_.size() < MAX_UPSTREAM_DEPS && !downstream_readable_) {
    EnableStreamRead();
  }
}

void SsConnection::disconnected(asio::error_code ec) {
  VLOG(2) << "Connection (server) " << connection_id()
          << " upstream: lost connection with: " << remote_domain()
          << " due to " << ec
          << " and data to write: " << downstream_.size();
  upstream_readable_ = false;
  upstream_writable_ = false;
  channel_->close();
  /* delay the socket's close because downstream is buffered */
  if (downstream_.empty()) {
    VLOG(3) << "Connection (server) " << connection_id()
            << " upstream: last data sent: shutting down";
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  } else {
    socket_.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
  }
}

std::shared_ptr<IOBuf> SsConnection::DecryptData(
    std::shared_ptr<IOBuf> cipherbuf) {
  std::shared_ptr<IOBuf> plainbuf = IOBuf::create(cipherbuf->length());
  plainbuf->reserve(0, cipherbuf->length());

  DumpHex("ERead->", cipherbuf.get());
  decoder_->decrypt(cipherbuf.get(), &plainbuf);
  DumpHex("PRead->", plainbuf.get());
  return plainbuf;
}

std::shared_ptr<IOBuf> SsConnection::EncryptData(std::shared_ptr<IOBuf> plainbuf) {
  std::shared_ptr<IOBuf> cipherbuf = IOBuf::create(plainbuf->length() + 100);
  cipherbuf->reserve(0, plainbuf->length() + 100);

  DumpHex("PWrite->", plainbuf.get());
  encoder_->encrypt(plainbuf.get(), &cipherbuf);
  DumpHex("EWrite->", cipherbuf.get());
  return cipherbuf;
}

}  // namespace ss
