// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#include <gtest/gtest-message.h>
#include <gtest/gtest.h>

#include <gmock/gmock.h>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include "third_party/boringssl/src/include/openssl/crypto.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#ifdef HAVE_CURL
#include <curl/curl.h>
ABSL_FLAG(std::string, proxy_type, "http", "proxy type, available: socks4, socks4a, socks5, socks5h, http");
#endif

#include "cli/cli_server.hpp"
#include "config/config.hpp"
#include "core/rand_util.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "feature.h"
#include "net/cipher.hpp"
#include "net/http_parser.hpp"
#include "net/iobuf.hpp"
#include "server/server_server.hpp"
#include "version.h"

#include "test_util.hpp"

namespace config {
const ProgramType pType = YASS_UNITTEST_DEFAULT;
}  // namespace config
using namespace config;

using namespace net;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

IOBuf g_send_buffer;
std::mutex g_in_provider_mutex;
std::unique_ptr<IOBuf> g_recv_buffer;
constexpr const std::string_view kConnectResponse = "HTTP/1.1 200 Connection established\r\n\r\n";

// openssl req -newkey rsa:1024 -keyout private_key.pem -x509 -out ca.cer -days 3650 -subj /C=XX
constexpr const std::string_view kCertificate = R"(
-----BEGIN CERTIFICATE-----
MIIB9jCCAV+gAwIBAgIUIO3vro1ogQk2h7OUSciXA1QKqZgwDQYJKoZIhvcNAQEL
BQAwDTELMAkGA1UEBhMCWFgwHhcNMjQwNTAxMDA1MzI3WhcNMzQwNDI5MDA1MzI3
WjANMQswCQYDVQQGEwJYWDCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEArmow
8HP8dNF4redHLbfN9BdUFIgHsOgydxnDkZ7BypQ8Q2Mys2SAwwWCyMC2jhZW1b8G
Pw9xCnjHaeVL63LfN6zUxJf/UyiMSFZIFcvR3M+PZBn8fzXTwPQZjXvyp5CA39rN
jBx5UiRlVPzEiM2TPfZsL8IXx6ZPW7fEyKUH1/UCAwEAAaNTMFEwHQYDVR0OBBYE
FO7GvhpAUoOLR7cRxiLcjcUZY2jyMB8GA1UdIwQYMBaAFO7GvhpAUoOLR7cRxiLc
jcUZY2jyMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADgYEAHRsLeR8+
7UbBuXh/05smxLLg29J5k+7SKj6L75qikPCnryHabPZznnETAiUag3uMMA681guh
xMi9tL7ERvFqAGjuoVFjTXCdmG62lOSp+pZrED7m+rZwXvXXh9DeSlS6qH1HQtIk
X8Ip5gh9SPTEHiSrq2HG8ZoMZg60sd+MmCA=
-----END CERTIFICATE-----
)";

constexpr const std::string_view kPrivateKeyPassword = "abc123";

constexpr const std::string_view kPrivateKey = R"(
-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIC1DBOBgkqhkiG9w0BBQ0wQTApBgkqhkiG9w0BBQwwHAQIYCKfVAAIczACAggA
MAwGCCqGSIb3DQIJBQAwFAYIKoZIhvcNAwcECFBjISAKzdo1BIICgFd5nHofaZ8R
fq/zg2eLjZbyS3gcHSx/ktk00fCQ6l4l6ka0cxDzplEto7O0AbVdcgSFnrRJ4VQa
g55iJeHu5ppGAoW53GyUrLYlDzt6VPvqH7/rouL9M8TSEpJIBXUwEWxdVa/1NYJY
WRi+ZQndhykIZa/UTkCwgreLql1sizJ+eb7Nw0VZ39PP/Nj90/gm6znAzQwPkYxA
+P7qcbqQmn1m6TJ+8X1hPNePdjJaEWqqsWvTWje3AsLFS6+GltHpsuDJTmSg9Iat
/f10kQ3uaIuil9lpC+tGxdKIc0bbRTXpJoknxxEUL1slmiM72LyUr311/kIArF+K
moDGw1iVXM0m8Y5IgLo0hrEzh+tYObytNFd3SQ8DnAvVMWyHNpdDxgTAuJ2aRN+n
/o/Wbxk1zz2KiFGXTT4e7afumoR5aoT4DXpJ6Qvqs6/O7jYrxTC3ErjgZPu0vHsH
KwJt9bYo6fJUxxYdaNR2sXzTFcFhpG0kLkBnbRLidpWbZ6Op0BNGGpivEe2mVmLZ
ICkT6UQ4FkGHup7AX+IuNFtvM/7X182QAm43cVi2HgeIjaTH4aln9HwZg+iYIZe6
XDaPa7d0QUV/7B+pfvgM7i4biBgzd6ubTwR1KP0NATnAhivuflklV4Nfxjrq8Os7
KxLhM/gx9zp8OzitrswtJhyGHXM99yC0PRXo256g6/kBiq0Wshihej2cy49AyPvn
6HLIp9f0p4RpLcF7RYy8uYSu4ZfgigWPeQ7qBtN/3xkLqhgOqGCkEMheR0kinmBD
N/IG+PMjBdw2nQ6ADXMiJqaqYcO78Bm6CJq/j9I2NnePAGsouyj0DK8De+VTNNIL
mNWq6Mvwz5w=
-----END ENCRYPTED PRIVATE KEY-----
)";

void GenerateRandContent(int size) {
  g_send_buffer.clear();
  g_send_buffer.reserve(0, size);

  RandBytes(g_send_buffer.mutable_data(), std::min(256, size));
  for (int i = 1; i < size / 256; ++i) {
    memcpy(g_send_buffer.mutable_data() + 256 * i, g_send_buffer.data(), 256);
  }
  g_send_buffer.append(size);

  g_recv_buffer = IOBuf::create(size);
}

class ContentProviderConnection : public RefCountedThreadSafe<ContentProviderConnection>, public Connection {
 public:
  static constexpr const ConnectionFactoryType Type = CONNECTION_FACTORY_CONTENT_PROVIDER;
  static constexpr const std::string_view Name = "content-provider";

 public:
  ContentProviderConnection(asio::io_context& io_context,
                            std::string_view remote_host_ips,
                            std::string_view remote_host_sni,
                            uint16_t remote_port,
                            bool upstream_https_fallback,
                            bool https_fallback,
                            bool enable_upstream_tls,
                            bool enable_tls,
                            SSL_CTX* upstream_ssl_ctx,
                            SSL_CTX* ssl_ctx)
      : Connection(io_context,
                   remote_host_ips,
                   remote_host_sni,
                   remote_port,
                   upstream_https_fallback,
                   https_fallback,
                   enable_upstream_tls,
                   enable_tls,
                   upstream_ssl_ctx,
                   ssl_ctx) {}

  ~ContentProviderConnection() override {
    VLOG(1) << "Connection (content-provider) " << connection_id() << " freed memory";
  }

  ContentProviderConnection(const ContentProviderConnection&) = delete;
  ContentProviderConnection& operator=(const ContentProviderConnection&) = delete;

  ContentProviderConnection(ContentProviderConnection&&) = delete;
  ContentProviderConnection& operator=(ContentProviderConnection&&) = delete;

  void start() { do_io(); }

  void close() {
    VLOG(1) << "Connection (content-provider) " << connection_id() << " disconnected";
    asio::error_code ec;
    downlink_->socket_.close(ec);
    on_disconnect();
  }

 private:
  asio::streambuf recv_buff_hdr;

  void read_http_request() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Read HTTP Request Header
    asio::async_read_until(
        downlink_->socket_, recv_buff_hdr, "\r\n\r\n", [this, self](asio::error_code ec, size_t bytes_transferred) {
          if (ec) {
            LOG(WARNING) << "Connection (content-provider) " << connection_id() << " Failed to transfer data: " << ec;
            shutdown();
            return;
          }

          std::string recv_buff_hdr_str(asio::buffers_begin(recv_buff_hdr.data()),
                                        asio::buffers_begin(recv_buff_hdr.data()) + bytes_transferred);
          span<const uint8_t> buf = as_bytes(make_span(recv_buff_hdr_str));

          HttpRequestParser parser;
          bool ok;
          int nparsed = parser.Parse(buf, &ok);
          if (nparsed) {
            VLOG(1) << "Connection (content-provider) " << connection_id() << " http request received: "
                    << std::string_view(reinterpret_cast<const char*>(buf.data()), nparsed);
          }
          if (!ok) {
            LOG(WARNING) << "Connection (content-provider) " << connection_id() << " Bad http request received: "
                         << std::string_view(reinterpret_cast<const char*>(buf.data()), nparsed);
            shutdown();
            return;
          }

          // append the rest of data to another stage
          recv_buff_hdr.consume(nparsed);

          if (recv_buff_hdr.size()) {
            memcpy(g_recv_buffer->mutable_tail(), &*asio::buffers_begin(recv_buff_hdr.data()), recv_buff_hdr.size());
            g_recv_buffer->append(recv_buff_hdr.size());
            VLOG(1) << "Connection (content-provider) " << connection_id()
                    << " read http data: " << recv_buff_hdr.size() << " bytes";
          }

          write_http_response_hdr1();
        });
  }

  void write_http_response_hdr1() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Write HTTP Response Header
    static const char http_response_hdr1[] = "HTTP/1.1 100 Continue\r\n\r\n";
    asio::async_write(downlink_->socket_, asio::const_buffer(http_response_hdr1, sizeof(http_response_hdr1) - 1),
                      [this, self](asio::error_code ec, size_t bytes_transferred) {
                        if (ec || bytes_transferred != sizeof(http_response_hdr1) - 1) {
                          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                                       << " Failed to transfer data: " << ec;
                          shutdown();
                          return;
                        }
                        VLOG(1) << "Connection (content-provider) " << connection_id()
                                << " write http header: " << bytes_transferred << " bytes.";
                        read_http_request_data();
                      });
  }

  void read_http_request_data() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Read HTTP Request Data
    asio::async_read(
        downlink_->socket_, tail_buffer(*g_recv_buffer), [this, self](asio::error_code ec, size_t bytes_transferred) {
          g_recv_buffer->append(bytes_transferred);
          VLOG(1) << "Connection (content-provider) " << connection_id() << " read http data: " << bytes_transferred
                  << " bytes";

          bytes_transferred = g_recv_buffer->length();
          if (ec || bytes_transferred != g_send_buffer.length()) {
            LOG(WARNING) << "Connection (content-provider) " << connection_id() << " Failed to transfer data: " << ec;
            shutdown();
            return;
          }
          write_http_response_hdr2();
        });
  }

  std::string http_response_hdr2;

  void write_http_response_hdr2() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Write HTTP Response Header, Part 2
    http_response_hdr2 = absl::StrFormat(
        "HTTP/1.1 200 OK\r\n"
        "Server: YASS/cp\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n\r\n",
        static_cast<unsigned long long>(g_send_buffer.length()));
    asio::async_write(downlink_->socket_, asio::const_buffer(http_response_hdr2.data(), http_response_hdr2.size()),
                      [this, self](asio::error_code ec, size_t bytes_transferred) {
                        if (ec || bytes_transferred != http_response_hdr2.size()) {
                          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                                       << " Failed to transfer data: " << ec;
                          shutdown();
                          return;
                        }
                        write_http_response_data();
                      });
  }

  void write_http_response_data() {
    // Write HTTP Response Data
    scoped_refptr<ContentProviderConnection> self(this);
    asio::async_write(
        downlink_->socket_, const_buffer(g_send_buffer), [this, self](asio::error_code ec, size_t bytes_transferred) {
          if (ec || bytes_transferred != g_send_buffer.length()) {
            LOG(WARNING) << "Connection (content-provider) " << connection_id() << " Failed to transfer data: " << ec;
          } else {
            VLOG(1) << "Connection (content-provider) " << connection_id() << " written: " << bytes_transferred
                    << " bytes";
          }
          shutdown();
        });
  }

  void do_io() {
    scoped_refptr<ContentProviderConnection> self(this);
    g_in_provider_mutex.lock();
    read_http_request();
  }

  void shutdown() {
    g_in_provider_mutex.unlock();
    asio::error_code ec;
    LOG(INFO) << "Connection (content-provider) " << connection_id() << " shutting down";
    downlink_->socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    if (ec) {
      LOG(WARNING) << "Connection (content-provider) " << connection_id() << " shutdown failure: " << ec;
    }
  }
};

using ContentProviderConnectionFactory = ConnectionFactory<ContentProviderConnection>;
using ContentProviderServer = ContentServer<ContentProviderConnectionFactory>;

#ifndef HAVE_CURL
void GenerateConnectRequest(std::string_view host, int port_num, IOBuf* buf) {
  std::string request_header = absl::StrFormat(
      "CONNECT %s:%d HTTP/1.1\r\n"
      "Host: packages.endpointdev.com:443\r\n"
      "User-Agent: curl/7.77.0\r\n"
      "Proxy-Connection: Close\r\n"
      "\r\n",
      host, port_num);
  buf->reserve(request_header.size(), 0);
  memcpy(buf->mutable_buffer(), request_header.c_str(), request_header.size());
  buf->prepend(request_header.size());
}
#endif

// [content provider] <== [ss server] <== [ss local] <== [content consumer]
class EndToEndTest : public ::testing::TestWithParam<cipher_method> {
 public:
  static void SetUpTestSuite() {
    // nop
  }

  static void TearDownTestSuite() {
    // nop
  }

  void SetUp() override {
    StartWorkThread();
    absl::SetFlag(&FLAGS_method, GetParam());
    StartBackgroundTasks();
  }

  void StartBackgroundTasks() {
    std::mutex m;
    bool done = false;
    asio::post(io_context_, [this, &m, &done]() {
      std::lock_guard<std::mutex> lk(m);
      auto ec = StartContentProvider(GetReusableEndpoint(), SOMAXCONN);
      ASSERT_FALSE(ec) << ec;
      ec = StartServer(GetReusableEndpoint(), SOMAXCONN);
      ASSERT_FALSE(ec) << ec;
      ec = StartLocal(server_endpoint_, GetReusableEndpoint(), SOMAXCONN);
      ASSERT_FALSE(ec) << ec;
      done = true;
    });
    while (true) {
      std::lock_guard<std::mutex> lk(m);
      if (done)
        break;
    }
  }

  void TearDown() override {
    StopClient();
    StopServer();
    StopContentProvider();
    work_guard_.reset();
    thread_->join();
    thread_.reset();
    local_server_.reset();
    server_server_.reset();
    content_provider_server_.reset();
  }

 protected:
  asio::ip::tcp::endpoint GetReusableEndpoint() const { return GetEndpoint(0); }

  asio::ip::tcp::endpoint GetEndpoint(int port_num) const {
    asio::error_code ec;
    auto addr = asio::ip::make_address(absl::GetFlag(FLAGS_ipv6_mode) ? "::1"sv : "127.0.0.1"sv, ec);
    CHECK(!ec) << ec;
    asio::ip::tcp::endpoint endpoint;
    endpoint.address(addr);
    endpoint.port(port_num);
    return endpoint;
  }

  void StartWorkThread() {
    thread_ = std::make_unique<std::thread>([this]() {
      if (!SetCurrentThreadName("background"s)) {
        PLOG(WARNING) << "failed to set thread name";
      }
      if (!SetCurrentThreadPriority(ThreadPriority::ABOVE_NORMAL)) {
        PLOG(WARNING) << "failed to set thread priority";
      }

      VLOG(1) << "background thread started";
      work_guard_ =
          std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
      io_context_.run();
      io_context_.restart();
      VLOG(1) << "background thread stopped";
    });
  }

  void SendRequestAndCheckResponse() {
    if (GetParam() == CRYPTO_SOCKS4 && absl::GetFlag(FLAGS_ipv6_mode)) {
      GTEST_SKIP() << "skipped as socks4 not supporint ipv6 address";
      return;
    }
#ifdef HAVE_CURL
    CURL* curl;
    char errbuf[CURL_ERROR_SIZE];
    std::stringstream err_ss;
    CURLcode ret;
    const uint8_t* data = g_send_buffer.data();
    IOBuf resp_buffer;
    resp_buffer.reserve(0, g_send_buffer.length());

    curl = curl_easy_init();
    ASSERT_TRUE(curl) << "curl initial failure";
    std::string url = absl::StrCat("http://localhost:", content_provider_endpoint_.port());
    // TODO A bug inside curl that it doesn't respect IPRESOLVE_V6
    // https://github.com/curl/curl/issues/11465
    if (absl::GetFlag(FLAGS_proxy_type) == "socks5"s) {
      if (absl::GetFlag(FLAGS_ipv6_mode)) {
        url = "http://[::1]:" + std::to_string(content_provider_endpoint_.port());
      } else {
        url = "http://127.0.0.1:" + std::to_string(content_provider_endpoint_.port());
      }
    }
    if (VLOG_IS_ON(1)) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    const long ip_version = absl::GetFlag(FLAGS_ipv6_mode) ? CURL_IPRESOLVE_V6 : CURL_IPRESOLVE_V4;
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, ip_version);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    std::string proxy_url = absl::StrCat("localhost:", local_endpoint_.port());
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url.c_str());
    if (absl::GetFlag(FLAGS_proxy_type) == "socks4"s) {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
    } else if (absl::GetFlag(FLAGS_proxy_type) == "socks4a"s) {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
    } else if (absl::GetFlag(FLAGS_proxy_type) == "socks5"s) {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, (long)CURLPROXY_SOCKS5);
    } else if (absl::GetFlag(FLAGS_proxy_type) == "socks5h"s) {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, (long)CURLPROXY_SOCKS5_HOSTNAME);
    } else if (absl::GetFlag(FLAGS_proxy_type) == "http"s) {
      curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    } else {
      LOG(FATAL) << "Invalid proxy type: " << absl::GetFlag(FLAGS_proxy_type);
    }
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
#if LIBCURL_VERSION_NUM >= 0x076200
    /* Added in 7.62.0. */
    /* The maximum buffer size allowed to be set is 2 megabytes. */
    curl_easy_setopt(curl, CURLOPT_UPLOAD_BUFFERSIZE, 2 * 1024 * 1024);
#endif
    /* we want to use our own read function */
    curl_read_callback r_callback = [](char* buffer, size_t size, size_t nmemb, void* clientp) -> size_t {
      const uint8_t** data = reinterpret_cast<const uint8_t**>(clientp);
      size_t copied = std::min<size_t>(g_send_buffer.tail() - *data, size * nmemb);
      if (copied) {
        memcpy(buffer, *data, copied);
        *data += copied;
        VLOG(1) << "Connection (content-consumer) write: " << copied << " bytes";
      }
      return copied;
    };
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, r_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    /* we want to use our own write function */
    curl_write_callback w_callback = [](char* data, size_t size, size_t nmemb, void* clientp) -> size_t {
      size_t copied = size * nmemb;
      VLOG(1) << "Connection (content-consumer) read: " << copied << " bytes";
      IOBuf* buf = reinterpret_cast<IOBuf*>(clientp);
      buf->reserve(0, copied);
      memcpy(buf->mutable_tail(), data, copied);
      buf->append(copied);
      return copied;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, w_callback);
    /* provide the size of the upload, we typecast the value to curl_off_t
       since we must be sure to use the correct data size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)g_send_buffer.length());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buffer);
#if LIBCURL_VERSION_NUM >= 0x075300
    /* Added in 7.10. Growing the buffer was added in 7.53.0. */
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
#endif
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/" LIBCURL_VERSION);
    ret = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    /* if the request did not complete correctly, show the error
    information. if no detailed error information was written to errbuf
    show the more generic information from curl_easy_strerror instead.
    */
    if (ret != CURLE_OK) {
      size_t len = strlen(errbuf);
      err_ss << "libcurl: (" << ret << ") ";
      if (len)
        err_ss << errbuf << ((errbuf[len - 1] != '\n') ? "\n" : "");
      else
        err_ss << curl_easy_strerror(ret);
    }
    ASSERT_EQ(ret, CURLE_OK) << "curl perform error: " << err_ss.str();
#else
    asio::io_context io_context;
    auto endpoint = local_endpoint_;

    // Connect to proxy server
    asio::ip::tcp::socket s(io_context);
    asio::error_code ec;
    s.connect(endpoint, ec);
    ASSERT_FALSE(ec) << ec;
    SetSocketTcpNoDelay(&s, ec);
    ASSERT_FALSE(ec) << ec;

    // Generate http 1.0 proxy header
    auto request_buf = IOBuf::create(SOCKET_BUF_SIZE);
    GenerateConnectRequest("localhost"sv, content_provider_endpoint_.port(), request_buf.get());

    // Write data after proxy header
    size_t written = asio::write(s, const_buffer(*request_buf), ec);
    VLOG(1) << "Connection (content-consumer) written: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, request_buf->length());

    // Read proxy response
    constexpr const int kConnectResponseLength = kConnectResponse.size();
    IOBuf response_buf;
    response_buf.reserve(0, kConnectResponseLength);
    size_t read = asio::read(s, asio::mutable_buffer(response_buf.mutable_tail(), kConnectResponseLength), ec);
    VLOG(1) << "Connection (content-consumer) read: " << read << " bytes";
    response_buf.append(read);
    ASSERT_EQ((int)read, kConnectResponseLength);

    // Check proxy response
    const char* response_buffer = reinterpret_cast<const char*>(response_buf.data());
    size_t response_buffer_length = response_buf.length();
    ASSERT_EQ((int)response_buffer_length, kConnectResponseLength);
    ASSERT_EQ(::testing::Bytes(response_buffer, response_buffer_length),
              ::testing::Bytes(kConnectResponse.data(), kConnectResponseLength));

    // Write HTTP Request Header
    std::string http_request_hdr = absl::StrFormat(
        "PUT / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Accept: */*\r\n"
        "Content-Length: %llu\r\n"
        "Expect: 100-continue\r\n\r\n",
        static_cast<unsigned long long>(g_send_buffer.length()));

    written = asio::write(s, asio::const_buffer(http_request_hdr.c_str(), http_request_hdr.size()), ec);
    VLOG(1) << "Connection (content-consumer) written hdr: " << http_request_hdr;
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, http_request_hdr.size());

    // Read HTTP Response "HTTP/1.1 100 Continue\r\n\r\n"
    static const char http_response_hdr1[] = "HTTP/1.1 100 Continue\r\n\r\n";

    // parse http response1
    asio::streambuf response_hdr1;
    read = asio::read_until(s, response_hdr1, "\r\n\r\n", ec);
    VLOG(1) << "Connection (content-consumer) read hdr1: " << read << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(read, response_hdr1.size());
    std::string response_hdr1_str(asio::buffers_begin(response_hdr1.data()),
                                  asio::buffers_begin(response_hdr1.data()) + read);
    EXPECT_EQ(response_hdr1_str, std::string(http_response_hdr1));

    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(read, sizeof(http_response_hdr1) - 1);

    // Write HTTP Data
    written = asio::write(s, const_buffer(g_send_buffer), ec);
    VLOG(1) << "Connection (content-consumer) written upload data: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, g_send_buffer.length());

    // Read HTTP Response Header
    // such as
    //    HTTP/1.1 200 OK
    //    Server: asio/1.0.0
    //    Content-Type: application/octet-stream
    //    Content-Length: 4096
    //    Connection: close
    asio::streambuf response_hdr2;
    read = asio::read_until(s, response_hdr2, "\r\n\r\n", ec);
    EXPECT_FALSE(ec) << ec;
    VLOG(1) << "Connection (content-consumer) read hdr2: " << read << " bytes";

    std::string response_hdr2_str(asio::buffers_begin(response_hdr2.data()),
                                  asio::buffers_begin(response_hdr2.data()) + read);
    span<const uint8_t> buf = as_bytes(make_span(response_hdr2_str));

    HttpResponseParser parser;
    bool ok;
    int nparsed = parser.Parse(buf, &ok);
    if (nparsed) {
      VLOG(1) << "Connection (content-consumer) http response hdr2 received: "
              << std::string_view(reinterpret_cast<const char*>(buf.data()), nparsed);
    }

    EXPECT_TRUE(ok) << "Connection (content-consumer) bad http response hdr2 received: "
                    << std::string_view(reinterpret_cast<const char*>(buf.data()), nparsed);

    EXPECT_EQ(parser.status_code(), 200) << "Bad response status";

    response_hdr2.consume(nparsed);
    EXPECT_EQ(parser.content_length(), g_send_buffer.length()) << "Dismatched content-length";

    // Read HTTP Data
    IOBuf resp_buffer;
    resp_buffer.reserve(0, g_send_buffer.length());

    if (response_hdr2.size()) {
      memcpy(resp_buffer.mutable_tail(), &*asio::buffers_begin(response_hdr2.data()), response_hdr2.size());
      resp_buffer.append(response_hdr2.size());
      VLOG(1) << "Connection (content-consumer) read: " << response_hdr2.size() << " bytes";
    }

    read = asio::read(s, tail_buffer(resp_buffer), ec);
    VLOG(1) << "Connection (content-consumer) read: " << read << " bytes";
    resp_buffer.append(read);
    read = resp_buffer.length();
    EXPECT_EQ(read, g_send_buffer.length());
    EXPECT_FALSE(ec) << ec;

    // Confirm EOF
    EXPECT_EQ(s.available(ec), 0u);
    EXPECT_FALSE(ec) << ec;

    // Confirm EOF (2)
    IOBuf eof_buffer;
    eof_buffer.reserve(0, SOCKET_DEBUF_SIZE);
    read = asio::read(s, tail_buffer(eof_buffer), ec);
    EXPECT_EQ(ec, asio::error::eof) << ec;
    EXPECT_EQ(read, 0u);
    VLOG(1) << "Connection (content-consumer) read EOF";

    // Shutdown socket to proxy server
    s.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    EXPECT_FALSE(ec) << ec;
    VLOG(1) << "Connection (content-consumer) shutdown";
#endif

    const char* buffer = reinterpret_cast<const char*>(resp_buffer.data());
    size_t buffer_length = resp_buffer.length();
    ASSERT_EQ(buffer_length, g_send_buffer.length());
    ASSERT_EQ(::testing::Bytes(buffer, buffer_length), ::testing::Bytes(g_send_buffer.data(), g_send_buffer.length()));

    {
      std::lock_guard<std::mutex> lk(g_in_provider_mutex);
      ASSERT_EQ(g_recv_buffer->length(), g_send_buffer.length());
      ASSERT_EQ(::testing::Bytes(g_recv_buffer->data(), g_recv_buffer->length()),
                ::testing::Bytes(g_send_buffer.data(), g_send_buffer.length()));
    }
  }

 private:
  asio::error_code StartContentProvider(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    content_provider_server_ = std::make_unique<ContentProviderServer>(io_context_);
    content_provider_server_->listen(endpoint, {}, backlog, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    content_provider_endpoint_ = content_provider_server_->endpoint();
    VLOG(1) << "content provider listening at " << content_provider_endpoint_;

    return ec;
  }

  void StopContentProvider() {
    VLOG(1) << "content provider stopping at " << content_provider_endpoint_;
    if (content_provider_server_) {
      content_provider_server_->stop();
    }
    g_recv_buffer->clear();
  }

  asio::error_code StartServer(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;
    server_server_ = std::make_unique<server::ServerServer>(io_context_, std::string_view(), std::string_view(),
                                                            uint16_t(), std::string_view(), kCertificate, kPrivateKey);
    server_server_->listen(endpoint, "localhost"sv, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    server_endpoint_ = server_server_->endpoint();
    VLOG(1) << "tcp server listening at " << server_endpoint_;

    return ec;
  }

  void StopServer() {
    if (server_server_) {
      server_server_->stop();
    }
  }

  asio::error_code StartLocal(asio::ip::tcp::endpoint remote_endpoint, asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    local_server_ =
        std::make_unique<cli::CliServer>(io_context_, absl::GetFlag(FLAGS_ipv6_mode) ? "::1"sv : "127.0.0.1"sv,
                                         "localhost"sv, remote_endpoint.port(), kCertificate);
    local_server_->listen(endpoint, {}, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      local_server_->stop();
      return ec;
    }

    local_endpoint_ = local_server_->endpoint();
    VLOG(1) << "local server listening at " << local_endpoint_ << " with upstream " << remote_endpoint;

    return ec;
  }

  void StopClient() {
    if (local_server_) {
      local_server_->stop();
    }
  }

 private:
  asio::io_context io_context_;
  std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
  std::unique_ptr<std::thread> thread_;

  std::unique_ptr<ContentProviderServer> content_provider_server_;
  asio::ip::tcp::endpoint content_provider_endpoint_;
  std::unique_ptr<server::ServerServer> server_server_;
  asio::ip::tcp::endpoint server_endpoint_;
  std::unique_ptr<cli::CliServer> local_server_;
  asio::ip::tcp::endpoint local_endpoint_;
};

class EndToEndTestPostQuantumnKyber : public EndToEndTest {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_enable_post_quantum_kyber, true);
    absl::SetFlag(&FLAGS_use_ml_kem, false);
    net::SSLServerSocket::TEST_set_post_quantumn_only_mode(true);
    EndToEndTest::SetUp();
  }
  void TearDown() override {
    EndToEndTest::TearDown();
    net::SSLServerSocket::TEST_set_post_quantumn_only_mode(false);
    absl::SetFlag(&FLAGS_enable_post_quantum_kyber, false);
    absl::SetFlag(&FLAGS_use_ml_kem, false);
  }
};

class EndToEndTestPostQuantumnMLKEM : public EndToEndTest {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_enable_post_quantum_kyber, true);
    absl::SetFlag(&FLAGS_use_ml_kem, true);
    net::SSLServerSocket::TEST_set_post_quantumn_only_mode(true);
    EndToEndTest::SetUp();
  }
  void TearDown() override {
    EndToEndTest::TearDown();
    net::SSLServerSocket::TEST_set_post_quantumn_only_mode(false);
    absl::SetFlag(&FLAGS_enable_post_quantum_kyber, false);
    absl::SetFlag(&FLAGS_use_ml_kem, false);
  }
};
}  // namespace

TEST_P(EndToEndTest, 4K) {
  GenerateRandContent(4096);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTest, 256K) {
  GenerateRandContent(256 * 1024);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTest, 1M) {
  GenerateRandContent(1024 * 1024);
  SendRequestAndCheckResponse();
}

static constexpr const cipher_method kCiphers[] = {
#define XX(num, name, string) CRYPTO_##name,
    CIPHER_METHOD_VALID_MAP(XX)
#undef XX
};

INSTANTIATE_TEST_SUITE_P(Ss,
                         EndToEndTest,
                         ::testing::ValuesIn(kCiphers),
                         [](const ::testing::TestParamInfo<cipher_method>& info) -> std::string {
                           return std::string(to_cipher_method_name(info.param));
                         });

TEST_P(EndToEndTestPostQuantumnKyber, 4K) {
  GenerateRandContent(4096);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTestPostQuantumnKyber, 256K) {
  GenerateRandContent(256 * 1024);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTestPostQuantumnKyber, 1M) {
  GenerateRandContent(1024 * 1024);
  SendRequestAndCheckResponse();
}

static constexpr const cipher_method kCiphersHttps[] = {
#define XX(num, name, string) CRYPTO_##name,
    CIPHER_METHOD_MAP_HTTPS(XX)
#undef XX
};

INSTANTIATE_TEST_SUITE_P(Ss,
                         EndToEndTestPostQuantumnKyber,
                         ::testing::ValuesIn(kCiphersHttps),
                         [](const ::testing::TestParamInfo<cipher_method>& info) -> std::string {
                           return std::string(to_cipher_method_name(info.param));
                         });

TEST_P(EndToEndTestPostQuantumnMLKEM, 4K) {
  GenerateRandContent(4096);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTestPostQuantumnMLKEM, 256K) {
  GenerateRandContent(256 * 1024);
  SendRequestAndCheckResponse();
}

TEST_P(EndToEndTestPostQuantumnMLKEM, 1M) {
  GenerateRandContent(1024 * 1024);
  SendRequestAndCheckResponse();
}

INSTANTIATE_TEST_SUITE_P(Ss,
                         EndToEndTestPostQuantumnMLKEM,
                         ::testing::ValuesIn(kCiphersHttps),
                         [](const ::testing::TestParamInfo<cipher_method>& info) -> std::string {
                           return std::string(to_cipher_method_name(info.param));
                         });

#if BUILDFLAG(IS_IOS)
extern "C" int xc_main();
int xc_main() {
  int argc = 1;
  char* argv[] = {(char*)"xc_main", nullptr};
#else
int main(int argc, char** argv) {
#endif
#ifndef _WIN32
  // setup signal handler
  signal(SIGPIPE, SIG_IGN);

  /* Block SIGPIPE in all threads, this can happen if a thread calls write on
     a closed pipe. */
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  sigset_t saved_mask;
  if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
    perror("pthread_sigmask failed");
    return -1;
  }
#endif

  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

#ifdef _WIN32
  // Disable all of the possible ways Windows conspires to make automated
  // testing impossible.
  ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
  ::_set_error_mode(_OUT_TO_STDERR);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif

  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetFlag(&FLAGS_v, 0);
  absl::SetFlag(&FLAGS_ipv6_mode, false);

  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);

  // first line of logging
  LOG(WARNING) << "Application starting: " << YASS_APP_TAG << " type: " << ProgramTypeToStr(pType);
  LOG(WARNING) << "Last Change: " << YASS_APP_LAST_CHANGE;
  LOG(WARNING) << "Features: " << YASS_APP_FEATURES;
#ifndef NDEBUG
  LOG(WARNING) << "Debug build (NDEBUG not #defined)\n";
#endif

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

#ifdef HAVE_CURL
  curl_global_init(CURL_GLOBAL_ALL);
#endif

  if (absl::GetFlag(FLAGS_ipv6_mode)) {
    CHECK(Net_ipv6works()) << "IPv6 stack is required but not available";
  }

  absl::SetFlag(&FLAGS_private_key_password, kPrivateKeyPassword);

  int ret = RUN_ALL_TESTS();

  PrintMallocStats();
  PrintCliStats();

#ifdef HAVE_CURL
  curl_global_cleanup();
#endif

  return ret;
}
