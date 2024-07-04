// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/resolver.hpp"

#include "config/config_network.hpp"
#include "core/utils.hpp"
#include "net/doh_resolver.hpp"
#include "net/dot_resolver.hpp"

#ifdef HAVE_C_ARES
#include "net/c-ares.hpp"
#endif

namespace net {

class Resolver::ResolverImpl {
 public:
  ResolverImpl(asio::io_context& io_context)
      : io_context_(io_context),
        doh_resolver_(nullptr),
        dot_resolver_(nullptr),
#ifdef HAVE_C_ARES
        resolver_(nullptr)
#else
        resolver_(io_context)
#endif
  {
  }

  int Init() {
    doh_url_ = absl::GetFlag(FLAGS_doh_url);
    if (!doh_url_.empty()) {
      doh_resolver_ = DoHResolver::Create(io_context_);
      return doh_resolver_->Init(doh_url_, 10000);
    }
    dot_host_ = absl::GetFlag(FLAGS_dot_host);
    if (!dot_host_.empty()) {
      dot_resolver_ = DoTResolver::Create(io_context_);
      return dot_resolver_->Init(dot_host_, 10000);
    }
#ifdef HAVE_C_ARES
    resolver_ = CAresResolver::Create(io_context_);
    return resolver_->Init(5000);
#else
    return 0;
#endif
  }

  void Cancel() {
    if (!doh_url_.empty()) {
      if (doh_resolver_) {
        doh_resolver_->Cancel();
      }
      return;
    }
    if (!dot_host_.empty()) {
      if (dot_resolver_) {
        dot_resolver_->Cancel();
      }
      return;
    }
#ifdef HAVE_C_ARES
    if (resolver_) {
      resolver_->Cancel();
    }
#else
    resolver_.cancel();
#endif
  }

  void Reset() {
    if (!doh_url_.empty()) {
      doh_resolver_.reset();
      return;
    }
    if (!dot_host_.empty()) {
      dot_resolver_.reset();
      return;
    }
#ifdef HAVE_C_ARES
    resolver_.reset();
#endif
  }

  void AsyncResolve(const std::string& host_name, int port, AsyncResolveCallback cb) {
    if (!doh_url_.empty()) {
      doh_resolver_->AsyncResolve(host_name, port, cb);
      return;
    }
    if (!dot_host_.empty()) {
      dot_resolver_->AsyncResolve(host_name, port, cb);
      return;
    }
#ifdef HAVE_C_ARES
    resolver_->AsyncResolve(host_name, std::to_string(port), cb);
#else
    resolver_.async_resolve(Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(), host_name,
                            std::to_string(port), cb);
#endif
  }

 private:
  asio::io_context& io_context_;
  std::string doh_url_;
  std::string dot_host_;

  scoped_refptr<DoHResolver> doh_resolver_;
  scoped_refptr<DoTResolver> dot_resolver_;

#ifdef HAVE_C_ARES
  scoped_refptr<CAresResolver> resolver_;
#else
  asio::ip::tcp::resolver resolver_;
#endif
};

Resolver::Resolver(asio::io_context& io_context) : impl_(new ResolverImpl(io_context)) {}

Resolver::~Resolver() {
  delete impl_;
}

int Resolver::Init() {
  return impl_->Init();
}

void Resolver::Cancel() {
  impl_->Cancel();
}

void Resolver::Reset() {
  impl_->Reset();
}

void Resolver::AsyncResolve(const std::string& host_name, int port, AsyncResolveCallback cb) {
  impl_->AsyncResolve(host_name, port, cb);
}

}  // namespace net
