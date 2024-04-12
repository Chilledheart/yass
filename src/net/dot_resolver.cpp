// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include "net/dot_resolver.hpp"

#include "core/utils.hpp"
#include "net/dns_addrinfo_helper.hpp"
#include "net/dns_message.hpp"
#include "net/ssl_socket.hpp"
#include "net/x509_util.hpp"
#include "url/gurl.h"

#define CURL_TIMEOUT_RESOLVE                      \
  300 /* when using asynch methods, we allow this \
         many seconds for a name resolve */

namespace net {

static constexpr const int kDoTPort = 853;

using namespace dns_message;

DoTResolver::DoTResolver(asio::io_context& io_context)
    : io_context_(io_context), resolver_(io_context), resolve_timer_(io_context) {}

DoTResolver::~DoTResolver() {
  Destroy();
  VLOG(1) << "DoT Resolver freed memory";
}

int DoTResolver::Init(const std::string& dot_host, int timeout_ms) {
  timeout_ms_ = timeout_ms ? timeout_ms : CURL_TIMEOUT_RESOLVE * 1000;
  dot_host_ = dot_host;

  asio::error_code ec;
  SetupSSLContext(ec);
  if (ec) {
    LOG(WARNING) << "Init OpenSSL Context Failure: " << ec;
    return -1;
  }

  init_ = true;

  return 0;
}

void DoTResolver::SetupSSLContext(asio::error_code& ec) {
  ssl_ctx_.reset(::SSL_CTX_new(::TLS_client_method()));
  SSL_CTX* ctx = ssl_ctx_.get();
  if (!ctx) {
    print_openssl_error();
    ec = asio::error::no_memory;
    return;
  }

  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_ALL, true);

  SSL_CTX_set_options(ctx, options.set_mask);
  SSL_CTX_clear_options(ctx, options.clear_mask);

  CHECK(SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION));
  CHECK(SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION));

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, ::SSL_CTX_get_verify_callback(ctx));
  SSL_CTX_set_reverify_on_resume(ctx, 1);
  if (ec) {
    return;
  }

  int ret;
  std::vector<unsigned char> alpn_vec = {2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
  // TODO support http2
  alpn_vec = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
  ret = SSL_CTX_set_alpn_protos(ctx, alpn_vec.data(), alpn_vec.size());
  static_cast<void>(ret);
  DCHECK_EQ(ret, 0);
  if (ret) {
    print_openssl_error();
    ec = asio::error::access_denied;
    return;
  }
  VLOG(1) << "Alpn support (client) enabled";

  ssl_socket_data_index_ = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  SSL_CTX_set_timeout(ctx, 1 * 60 * 60 /* one hour */);

  SSL_CTX_set_grease_enabled(ctx, 1);

  // Deduplicate all certificates minted from the SSL_CTX in memory.
  SSL_CTX_set0_buffer_pool(ctx, x509_util::GetBufferPool());

  load_ca_to_ssl_ctx(ctx);
}

void DoTResolver::Cancel() {
  if (!init_) {
    return;
  }
  DCHECK(init_);
  cb_ = nullptr;

  resolver_.cancel();
  resolve_timer_.cancel();

  auto reqs = std::move(reqs_);
  for (auto req : reqs) {
    req->close();
  }

  auto* addrinfo = addrinfo_;
  addrinfo_ = nullptr;
  addrinfo_freedup(addrinfo);
}

void DoTResolver::Destroy() {
  if (!init_) {
    return;
  }
  Cancel();
}

void DoTResolver::AsyncResolve(const std::string& host, int port, AsyncResolveCallback cb) {
  DCHECK(init_) << "Init should be called before use";
  DCHECK(done_) << "Another resolve is in progress";

  host_ = host;
  port_ = port;
  cb_ = std::move(cb);
  scoped_refptr<DoTResolver> self(this);

  done_ = false;
  resolve_timer_.expires_after(std::chrono::milliseconds(timeout_ms_));
  resolve_timer_.async_wait([this, self](asio::error_code ec) {
    if (ec == asio::error::operation_aborted) {
      return;
    }
    if (done_) {
      return;
    }
    VLOG(1) << "DoT Resolver timed out";
    OnDoneRequest(asio::error::timed_out);
  });

  // use cached dns resolve results
  if (!endpoints_.empty()) {
    DoRequest(Net_ipv6works(), endpoints_.front());
    return;
  }

  asio::error_code ec;
  auto addr = asio::ip::make_address(dot_host_.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address) {
    VLOG(1) << "DoT Resolve resolved ip-like address (post-resolved): " << addr.to_string();
    endpoints_.emplace_back(addr, kDoTPort);
    DoRequest(Net_ipv6works(), endpoints_.front());
    return;
  }

  resolver_.async_resolve(
      Net_ipv6works() ? asio::ip::tcp::unspec() : asio::ip::tcp::v4(), dot_host_, std::to_string(kDoTPort),
      [this, self](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
        // Cancelled, safe to ignore
        if (UNLIKELY(ec == asio::error::operation_aborted)) {
          return;
        }
        if (ec) {
          DCHECK(reqs_.empty());
          OnDoneRequest(ec);
          return;
        }
        for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
          endpoints_.push_back(*iter);
          VLOG(1) << "DoT Resolve found ip address (post-resolved): " << endpoints_.back().address().to_string();
        }
        DCHECK(!endpoints_.empty());
        DoRequest(Net_ipv6works(), endpoints_.front());
      });
}

void DoTResolver::DoRequest(bool enable_ipv6, const asio::ip::tcp::endpoint& endpoint) {
  scoped_refptr<DoTResolver> self(this);
  VLOG(2) << "DoT Query Request IPv4: " << host_;
  auto req = DoTRequest::Create(ssl_socket_data_index_, io_context_, endpoint, dot_host_, kDoTPort, ssl_ctx_.get());
  req->DoRequest(DNS_TYPE_A, host_, port_, [this, self](const asio::error_code& ec, struct addrinfo* addrinfo) {
    VLOG(2) << "DoT Query Request IPv4: " << host_ << " Done: " << ec;
    /* ipv4 address comes first */
    if (addrinfo) {
      struct addrinfo* next_addrinfo = addrinfo_;
      addrinfo_ = addrinfo;
      while (addrinfo->ai_next) {
        addrinfo = addrinfo->ai_next;
      }
      addrinfo->ai_next = next_addrinfo;
    }
    reqs_.pop_front();
    OnDoneRequest(ec);
  });
  reqs_.push_back(req);
  if (enable_ipv6) {
    VLOG(2) << "DoT Query Request IPv6: " << host_;
    auto req = DoTRequest::Create(ssl_socket_data_index_, io_context_, endpoint, dot_host_, kDoTPort, ssl_ctx_.get());
    req->DoRequest(DNS_TYPE_AAAA, host_, port_, [this, self](const asio::error_code& ec, struct addrinfo* addrinfo) {
      VLOG(2) << "DoT Query Request IPv6: " << host_ << " Done: " << ec;
      /* ipv6 address comes later */
      if (addrinfo_) {
        struct addrinfo* prev_addrinfo = addrinfo_;
        while (prev_addrinfo->ai_next) {
          prev_addrinfo = prev_addrinfo->ai_next;
        }
        prev_addrinfo->ai_next = addrinfo;
      } else {
        addrinfo_ = addrinfo;
      }
      reqs_.pop_back();
      // FIXME should we ignore the failure?
      OnDoneRequest(ec);
    });
    reqs_.push_back(req);
  }
}

void DoTResolver::OnDoneRequest(asio::error_code ec) {
  if (ec) {
    auto reqs = std::move(reqs_);
    for (auto req : reqs) {
      req->close();
    }
  }
  if (!reqs_.empty()) {
    VLOG(3) << "DoTResolver pending on another request";
    return;
  }
  if (done_) {
    return;
  }
  done_ = true;
  resolve_timer_.cancel();

  auto* addrinfo = addrinfo_;
  addrinfo_ = nullptr;
  auto results = asio::ip::tcp::resolver::results_type::create(addrinfo, host_, std::to_string(port_));
  addrinfo_freedup(addrinfo);

  if (results.empty() && !ec) {
    ec = asio::error::host_not_found;
  }

  std::ostringstream ss;
  for (auto iter = std::begin(results); iter != std::end(results); ++iter) {
    const asio::ip::tcp::endpoint& endpoint = *iter;
    ss << endpoint << " ";
  }
  VLOG(1) << "DoT: Resolved " << host_ << ":" << port_ << " to: [ " << ss.str() << " ]";

  if (auto cb = std::move(cb_)) {
    cb(ec, results);
  }
}

}  // namespace net
