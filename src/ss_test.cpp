// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <openssl/crypto.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#include "cli/cli_server.hpp"
#include "config/config.hpp"
#include "core/cipher.hpp"
#include "core/iobuf.hpp"
#include "core/rand_util.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "core/stringprintf.hpp"
#include "server/server_server.hpp"

#include "test_util.hpp"

namespace {

IOBuf g_send_buffer;
std::mutex g_in_provider_mutex;
std::unique_ptr<IOBuf> g_recv_buffer;
const char kConnectResponse[] = "HTTP/1.1 200 Connection established\r\n\r\n";

// openssl req -newkey rsa:1024 -keyout pkey.pem -x509 -out cert.crt -days 3650 -nodes -subj /C=XX
const char kCertificate[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIB9jCCAV+gAwIBAgIUM03bTKd+A2WwrfolXJC+L9AsxI8wDQYJKoZIhvcNAQEL\n"
"BQAwDTELMAkGA1UEBhMCWFgwHhcNMjMwMTI5MjA1MDU5WhcNMzMwMTI2MjA1MDU5\n"
"WjANMQswCQYDVQQGEwJYWDCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA3GGZ\n"
"pQbdPh22uCMIes5GUJfDqsAda5I7JeUt1Uq0KebsQ1rxM9QUgzsvVktYqKGxZW57\n"
"djPlcWthfUGlUQAPpZ3/njWter81vy7oj/SfiEvZXk9LyrEA7vf9XIpFJhVrucpI\n"
"wzX1KmQAJdpc0yYmVvG+59PNI9SF6mGUWDGBhukCAwEAAaNTMFEwHQYDVR0OBBYE\n"
"FPFt885ocZzO8rQ7gu6vr+i/nrEEMB8GA1UdIwQYMBaAFPFt885ocZzO8rQ7gu6v\n"
"r+i/nrEEMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADgYEApAMdus13\n"
"9A4wGjtSmI1qsh/+nBeVrQWUOQH8eb0Oe7dDYg58EtzjhlvpLQ7nAOVO8fsioja7\n"
"Hine/sjADd7nGUrsIP+JIxplayLXcrP37KwaWxyRHoh/Bqa+7D3RpCv0SrNsIvlt\n"
"yyvnIm8njIJSin7Vf4tD1PfY6Obyc8ygUSw=\n"
"-----END CERTIFICATE-----\n";
const char kPrivateKey[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBANxhmaUG3T4dtrgj\n"
"CHrORlCXw6rAHWuSOyXlLdVKtCnm7ENa8TPUFIM7L1ZLWKihsWVue3Yz5XFrYX1B\n"
"pVEAD6Wd/541rXq/Nb8u6I/0n4hL2V5PS8qxAO73/VyKRSYVa7nKSMM19SpkACXa\n"
"XNMmJlbxvufTzSPUhephlFgxgYbpAgMBAAECgYBprRuB+NKqcJEnpxTv3m31Q3D+\n"
"NfVlmc9nEohx2MqftS3h9n/m/HGBpCXE2YiABFkObHYjbis9weITsCDXwJG/UtEO\n"
"yv8DqTEVcFYAg7fBu6dRaPsAvuDt4MDnk82/M9ZbtXqG7REp7hMxk3uKSThUfMoR\n"
"lIJiUhu2TCHHsw25IQJBAPzNPtn4peug9wXQcd7n1fFXOvjELHX011JFgAYQRoJu\n"
"Jmdfpz0+mzqLaagIPEENqwfGAMYkfOSPJWQhfcpeq70CQQDfK1qNNCqJzciGD/K7\n"
"xBEliKFGTKBI0Ru5FVPJQjEzorez/sIjsPqqEvfenJ6LyyfKgeaoWpsB5sRnn+Li\n"
"ZESdAkANa3vVqFxueLoERf91fMsfp6jKwec2T8wKYwQbzktf6ycAv9Qp7SPiZLo0\n"
"IFPKhEY7AGjUG+XBYFP0z85UqtflAkBSp8r8+3I54dbAGI4NjzvOjAE3eU/wSEqd\n"
"TVHf+70fY8foSZX8BCOC9E2LzLRIEHFnZp9YgV5h4OejfatZsEtdAkAZU+hVlaJD\n"
"GxqmgkJNSUluJFKduxyhdSB/cPmN0N/CFPxgfMEuRuJW3+POWfzQvLCxQ6m1+BpG\n"
"kMmiIVi25B8z\n"
"-----END PRIVATE KEY-----\n";

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

class ContentProviderConnection  : public RefCountedThreadSafe<ContentProviderConnection>,
                                   public Connection {
 public:
  ContentProviderConnection(asio::io_context& io_context,
                            const std::string& remote_host_name,
                            uint16_t remote_port,
                            bool upstream_https_fallback,
                            bool https_fallback,
                            bool enable_upstream_tls,
                            bool enable_tls,
                            asio::ssl::context *upstream_ssl_ctx,
                            asio::ssl::context *ssl_ctx)
      : Connection(io_context, remote_host_name, remote_port,
                   upstream_https_fallback, https_fallback,
                   enable_upstream_tls, enable_tls,
                   upstream_ssl_ctx, ssl_ctx) {}

  ~ContentProviderConnection() override {
    VLOG(1) << "Connection (content-provider) " << connection_id() << " freed memory";
  }

  ContentProviderConnection(const ContentProviderConnection&) = delete;
  ContentProviderConnection& operator=(const ContentProviderConnection&) = delete;

  ContentProviderConnection(ContentProviderConnection&&) = delete;
  ContentProviderConnection& operator=(ContentProviderConnection&&) = delete;

  void start() override {
    // FIXME check out why testcases fail with nonblocking mode
    asio::error_code ec;
    socket_.native_non_blocking(false, ec);
    socket_.non_blocking(false, ec);
    do_io();
  }

  void close() override {
    VLOG(1) << "Connection (content-provider) " << connection_id()
            << " disconnected";
    asio::error_code ec;
    socket_.close(ec);
    auto cb = std::move(disconnect_cb_);
    disconnect_cb_ = nullptr;
    if (cb) {
      cb();
    }
  }

 private:
  asio::streambuf recv_buff_hdr;

  void read_http_request() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Read HTTP Request Header
    asio::async_read_until(socket_, recv_buff_hdr, "\r\n\r\n",
      [this, self](asio::error_code ec, size_t bytes_transferred) {
        if (ec) {
          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                       << " Failed to transfer data: " << ec;
          shutdown();
          return;
        }
        VLOG(1) << "Connection (content-provider) " << connection_id()
                << " read http header: " << bytes_transferred << " bytes";
        // FIXME parse http header
#if 0
        std::string recv_buff_hdr_str(asio::buffers_begin(recv_buff_hdr.data()),
                                      asio::buffers_begin(recv_buff_hdr.data()) + bytes_transferred);
#endif

        // append the rest of data to another stage
        recv_buff_hdr.consume(bytes_transferred);
        if (recv_buff_hdr.size()) {
          memcpy(g_recv_buffer->mutable_tail(), &*asio::buffers_begin(recv_buff_hdr.data()), recv_buff_hdr.size());
          g_recv_buffer->append(recv_buff_hdr.size());
          VLOG(1) << "Connection (content-provider) " << connection_id()
                  << " read http data: " << bytes_transferred << " bytes";
        }

        write_http_response_hdr1();
    });
  }

  void write_http_response_hdr1() {
    scoped_refptr<ContentProviderConnection> self(this);
    // Write HTTP Response Header
    static const char http_response_hdr1[] = "HTTP/1.1 100 Continue\r\n\r\n";
    asio::async_write(socket_, asio::const_buffer(http_response_hdr1,
                                                  sizeof(http_response_hdr1) - 1),
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
    asio::async_read(socket_, tail_buffer(*g_recv_buffer),
      [this, self](asio::error_code ec, size_t bytes_transferred) {
        g_recv_buffer->append(bytes_transferred);
        VLOG(1) << "Connection (content-provider) " << connection_id()
                << " read http data: " << bytes_transferred << " bytes";

        bytes_transferred = g_recv_buffer->length();
        if (ec || bytes_transferred != g_send_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                       << " Failed to transfer data: " << ec;
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
    http_response_hdr2 = StringPrintf(
      "HTTP/1.1 200 OK\r\n"
      "Server: asio/1.0.0\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Length: %llu\r\n"
      "Connection: close\r\n\r\n", static_cast<unsigned long long>(g_send_buffer.length()));
    asio::async_write(socket_, asio::const_buffer(http_response_hdr2.data(),
                                                  http_response_hdr2.size()),
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
    asio::async_write(socket_, const_buffer(g_send_buffer),
      [this, self](asio::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred != g_send_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                       << " Failed to transfer data: " << ec;
        } else {
          VLOG(1) << "Connection (content-provider) " << connection_id()
                  << " written: " << bytes_transferred << " bytes";
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
    asio::error_code ec;
    g_in_provider_mutex.unlock();
    socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    LOG(WARNING) << "Connection (content-provider) " << connection_id()
                 << " Shutting down";
    if (ec) {
      LOG(WARNING) << "Connection (content-provider) " << connection_id()
                   << " shutdown failure: " << ec;
    }
  }
};

class ContentProviderConnectionFactory : public ConnectionFactory {
 public:
   using ConnectionType = ContentProviderConnection;
   template<typename... Args>
   scoped_refptr<ConnectionType> Create(Args&&... args) {
     return MakeRefCounted<ConnectionType>(std::forward<Args>(args)...);
   }
   const char* Name() override { return "content-provider"; }
   const char* ShortName() override { return "cp"; }
};

typedef ContentServer<ContentProviderConnectionFactory> ContentProviderServer;

#ifndef HAVE_CURL
void GenerateConnectRequest(std::string host, int port_num, IOBuf *buf) {
  std::string request_header = StringPrintf(
      "CONNECT %s:%d HTTP/1.1\r\n"
      "Host: packages.endpointdev.com:443\r\n"
      "User-Agent: curl/7.77.0\r\n"
      "Proxy-Connection: Keep-Alive\r\n"
      "\r\n", host.c_str(), port_num);
  buf->reserve(request_header.size(), 0);
  memcpy(buf->mutable_buffer(), request_header.c_str(), request_header.size());
  buf->prepend(request_header.size());
}
#endif

// [content provider] <== [ss server] <== [ss local] <== [content consumer]
class SsEndToEndTest : public ::testing::Test {
 public:
  void SetUp() override {
    StartWorkThread();
    absl::SetFlag(&FLAGS_password, "<dummy-password>");
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
  asio::ip::tcp::endpoint GetReusableEndpoint() const {
    return GetEndpoint(0);
  }

  asio::ip::tcp::endpoint GetEndpoint(int port_num) const {
    asio::error_code ec;
    auto addr = asio::ip::make_address(
        absl::GetFlag(FLAGS_ipv6_mode) ?  "::1" : "127.0.0.1", ec);
    CHECK(!ec) << ec;
    asio::ip::tcp::endpoint endpoint;
    endpoint.address(addr);
    endpoint.port(port_num);
    return endpoint;
  }

  void StartWorkThread() {
    thread_ = std::make_unique<std::thread>([this]() {
      asio::error_code ec;
      VLOG(1) << "background thread started";

      work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(io_context_.get_executor());
      io_context_.run();
      io_context_.restart();

      VLOG(1) << "background thread stopped";
    });

    asio::post(io_context_, [this]() {
      if (!SetThreadName(thread_->native_handle(), "background")) {
        PLOG(WARNING) << "failed to set thread name";
      }
      if (!SetThreadPriority(thread_->native_handle(), ThreadPriority::ABOVE_NORMAL)) {
        PLOG(WARNING) << "failed to set thread priority";
      }
    });
  }

  void SendRequestAndCheckResponse() {
#ifdef HAVE_CURL
    CURL *curl;
    char errbuf[CURL_ERROR_SIZE];
    std::stringstream err_ss;
    CURLcode ret;
    const uint8_t* data = g_send_buffer.data();
    IOBuf resp_buffer;

    curl = curl_easy_init();
    ASSERT_TRUE(curl) << "curl initial failure";
    std::string url = "http://localhost:" + std::to_string(content_provider_endpoint_.port());
    std::string proxy_url = "http://localhost:" + std::to_string(local_endpoint_.port());
    if (VLOG_IS_ON(1)) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    /* we want to use our own read function */
    curl_read_callback r_callback = [](char *buffer, size_t size, size_t nmemb, void *clientp) -> size_t {
      const uint8_t **data = reinterpret_cast<const uint8_t **>(clientp);
      size_t copied = std::min<size_t>(g_send_buffer.tail() - *data, size * nmemb);
      if (copied) {
        memcpy(buffer, *data, copied);
        *data += copied;
        VLOG(1) << "Connection (content-consumer) write: " << copied << " bytes";
      }
      return copied;
    };
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, r_callback);
    /* we want to use our own write function */
    curl_write_callback w_callback = [](char *data, size_t size, size_t nmemb, void *clientp) -> size_t {
      size_t copied = size * nmemb;
      VLOG(1) << "Connection (content-consumer) read: " << copied << " bytes";
      IOBuf* buf = reinterpret_cast<IOBuf*>(clientp);
      buf->reserve(0, copied);
      memcpy(buf->mutable_tail(), data, copied);
      buf->append(copied);
      return copied;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, w_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    /* provide the size of the upload, we typecast the value to curl_off_t
       since we must be sure to use the correct data size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)g_send_buffer.length());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buffer);
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

    // Generate http 1.0 proxy header
    auto request_buf = IOBuf::create(SOCKET_BUF_SIZE);
    GenerateConnectRequest("localhost", content_provider_endpoint_.port(),
                           request_buf.get());

    // Write data after proxy header
    size_t written = asio::write(s, const_buffer(*request_buf), ec);
    VLOG(1) << "Connection (content-consumer) written: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, request_buf->length());

    // Read proxy response
    constexpr int response_len = sizeof(kConnectResponse) - 1;
    IOBuf response_buf;
    response_buf.reserve(0, response_len);
    size_t read = asio::read(s, asio::mutable_buffer(response_buf.mutable_tail(), response_len), ec);
    VLOG(1) << "Connection (content-consumer) read: " << read << " bytes";
    response_buf.append(read);
    ASSERT_EQ((int)read, response_len);

    // Check proxy response
    const char* response_buffer = reinterpret_cast<const char*>(response_buf.data());
    size_t response_buffer_length = response_buf.length();
    ASSERT_EQ((int)response_buffer_length, response_len);
    ASSERT_EQ(::testing::Bytes(response_buffer, response_buffer_length),
              ::testing::Bytes(kConnectResponse, response_len));

    // Write HTTP Request Header
    std::string http_request_hdr =
      StringPrintf(
      "PUT / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Accept: */*\r\n"
      "Content-Length: %llu\r\n"
      "Expect: 100-continue\r\n\r\n", static_cast<unsigned long long>(g_send_buffer.length()));

    written = asio::write(s, asio::const_buffer(http_request_hdr.c_str(),
                                                http_request_hdr.size()), ec);
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
    VLOG(1) << "Connection (content-consumer) read hdr2: " << read << " bytes";
    EXPECT_FALSE(ec) << ec;
    // FIXME parse http response2 and extract content-length
#if 0
    std::string response_hdr2_str(asio::buffers_begin(response_hdr2.data()),
                                  asio::buffers_begin(response_hdr2.data()) + read);
#endif
    response_hdr2.consume(read);

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
    ASSERT_EQ(::testing::Bytes(buffer, buffer_length),
              ::testing::Bytes(g_send_buffer.data(), g_send_buffer.length()));

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
    content_provider_server_->listen(endpoint, std::string(), backlog, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    content_provider_endpoint_ = content_provider_server_->endpoint();
    VLOG(1) << "content provider listening at " << content_provider_endpoint_;

    return ec;
  }

  void StopContentProvider() {
    if (content_provider_server_) {
      content_provider_server_->stop();
    }
    g_recv_buffer->clear();
  }

  asio::error_code StartServer(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;
    server_server_ = std::make_unique<server::ServerServer>(io_context_,
                                                            std::string(),
                                                            uint16_t(),
                                                            std::string(),
                                                            std::string(kCertificate),
                                                            std::string(kPrivateKey));
    server_server_->listen(endpoint, "localhost", backlog, ec);

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

  asio::error_code StartLocal(asio::ip::tcp::endpoint remote_endpoint,
                              asio::ip::tcp::endpoint endpoint,
                              int backlog) {
    asio::error_code ec;

    local_server_ = std::make_unique<cli::CliServer>(io_context_,
                                                     "localhost",
                                                     remote_endpoint.port(),
                                                     kCertificate);
    local_server_->listen(endpoint, std::string(), backlog, ec);

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
}

#define XX(num, name, string) \
  TEST_F(SsEndToEndTest, name##_4K) { \
    absl::SetFlag(&FLAGS_method, CRYPTO_##name);        \
    StartBackgroundTasks(); \
    GenerateRandContent(4096); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_256K) { \
    absl::SetFlag(&FLAGS_method, CRYPTO_##name);        \
    StartBackgroundTasks(); \
    GenerateRandContent(256 * 1024); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_1M) { \
    absl::SetFlag(&FLAGS_method, CRYPTO_##name);        \
    StartBackgroundTasks(); \
    GenerateRandContent(1024 * 1024); \
    SendRequestAndCheckResponse(); \
  }
CIPHER_METHOD_VALID_MAP(XX)
#undef XX

int main(int argc, char **argv) {
  SetExecutablePath(argv[0]);
  std::string exec_path;
  if (!GetExecutablePath(&exec_path)) {
    return -1;
  }

  absl::InitializeSymbolizer(exec_path.c_str());
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetFlag(&FLAGS_v, 0);
  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_ipv6_mode, false);

  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);
  IoQueue::set_allow_merge(absl::GetFlag(FLAGS_io_queue_allow_merge));

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif

  CRYPTO_library_init();

#ifdef HAVE_CURL
  curl_global_init(CURL_GLOBAL_ALL);
#endif

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  if (absl::GetFlag(FLAGS_ipv6_mode)) {
    CHECK(Net_ipv6works()) << "IPv6 stack is required but not available";
  }

  int ret = RUN_ALL_TESTS();

#ifdef HAVE_CURL
  curl_global_cleanup();
#endif

  return ret;
}
