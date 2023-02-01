// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include "core/c-ares.hpp"
class CAresLibLoader {
 public:
  CAresLibLoader() {
    ::ares_library_init(ARES_LIB_INIT_ALL);
    LOG(INFO) << "C-Ares Loaded";
  }
  ~CAresLibLoader() {
    ::ares_library_cleanup();
    LOG(INFO) << "C-Ares Unloaded";
  }
};

CAresResolver::CAresResolver(asio::io_context &io_context) :
  io_context_(io_context), resolve_timer_(io_context) {
  static CAresLibLoader loader;
}

CAresResolver::~CAresResolver() {
  Destroy();
  VLOG(2) << "c-ares resolver freed memory";
}

int CAresResolver::Init(int timeout_ms, int retries) {
  ares_opts_.lookups = lookups_;
  ares_opts_.timeout = timeout_ms_ = timeout_ms;
  ares_opts_.tries = retries;
  ares_opts_.sock_state_cb_data = this;
  ares_opts_.sock_state_cb = &CAresResolver::OnSockState;
  int ret = ::ares_init_options(&channel_, &ares_opts_,
                                ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES |
                                ARES_OPT_SOCK_STATE_CB | ARES_OPT_LOOKUPS);
  if (ret) {
    LOG(WARNING) << "ares_init_options failure: " << ares_strerror(ret);
  }
  return ret;
}

void CAresResolver::Destroy() {
  ::ares_destroy(channel_);
}

void CAresResolver::OnSockState(void *arg, fd_t fd, int readable, int writable) {
  auto self = scoped_refptr(reinterpret_cast<CAresResolver*>(arg));
  auto iter = self->fd_map_.find(fd);
  if (!readable && !writable) {
    if (iter != self->fd_map_.end()) {
      iter->second->read_enable = false;
      iter->second->write_enable = false;
      self->fd_map_.erase(iter);
    }
    return;
  }
  if (iter == self->fd_map_.end()) {
    auto value = std::make_pair(fd, ResolverPerContext::Create(self->io_context_, fd));
    iter = self->fd_map_.insert(value).first;
  }
  auto ctx = iter->second;
  if (!ctx->read_enable && readable) {
    ctx->socket.async_wait(asio::ip::tcp::socket::wait_read,
      [self, ctx, fd](asio::error_code ec){
      if (!ctx->read_enable) {
        return;
      }
      if (ec) {
        return;
      }
      fd_t r = fd;
      fd_t w = ARES_SOCKET_BAD;
      ::ares_process_fd(self->channel_, r, w);
    });
  }
  if (!ctx->write_enable && writable) {
    ctx->socket.async_wait(asio::ip::tcp::socket::wait_write,
      [self, ctx, fd](asio::error_code ec){
      if (!ctx->write_enable) {
        return;
      }
      if (ec) {
        return;
      }
      fd_t r = ARES_SOCKET_BAD;
      fd_t w = fd;
      ::ares_process_fd(self->channel_, r, w);
    });
  }
  ctx->read_enable = readable;
  ctx->write_enable = writable;
}

void CAresResolver::AsyncResolve(const std::string& name, AsyncResolveCallback cb) {
  struct async_resolve_ctx {
    async_resolve_ctx(CAresResolver* s, AsyncResolveCallback c)
     : self(s), cb(c) {}
    scoped_refptr<CAresResolver> self;
    AsyncResolveCallback cb;
  };
  auto ctx = std::make_unique<async_resolve_ctx>(this, cb);
  ctx->self = this;
  ctx->cb = cb;
  ::ares_gethostbyname(channel_, name.c_str(), AF_INET,
    [](void *data, int status, int timeouts, struct hostent *hostent) {
      auto ctx = std::unique_ptr<async_resolve_ctx>(reinterpret_cast<async_resolve_ctx*>(data));
      auto self = ctx->self;
      auto cb = ctx->cb;
      asio::error_code ec;
      self->resolve_timer_.cancel(ec);
      if (timeouts > 0) {
        cb(nullptr, asio::error::timed_out);
        return;
      }
      if (status != ARES_SUCCESS) {
        cb(nullptr, asio::error::not_found);
        return;
      }
      cb(hostent, asio::error_code());
  }, ctx.release());
  struct timeval tv {};
  struct timeval *tvp = ::ares_timeout(channel_, nullptr, &tv);
  if (tvp) {
    resolve_timer_.expires_from_now(std::chrono::seconds(tvp->tv_sec) +
                                    std::chrono::microseconds(tvp->tv_usec));
  }
  scoped_refptr<CAresResolver> self(this);
  resolve_timer_.async_wait([self](
    asio::error_code ec){
      if (ec == asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        return;
      }
      fd_t r = ARES_SOCKET_BAD;
      fd_t w = ARES_SOCKET_BAD;
      ::ares_process_fd(self->channel_, r, w);
  });
}
