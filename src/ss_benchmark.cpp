// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#include <benchmark/benchmark.h>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_format.h>
#include <openssl/crypto.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#include "cli/cli_server.hpp"
#include "config/config.hpp"
#include "core/cipher.hpp"
#include "core/iobuf.hpp"
#include "core/rand_util.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "server/server_server.hpp"

namespace {

IOBuf g_send_buffer;
std::mutex g_in_provider_mutex;
std::mutex g_in_consumer_mutex;
std::unique_ptr<IOBuf> g_recv_buffer;
const char kConnectResponse[] = "HTTP/1.1 200 Connection established\r\n\r\n";
const int kIOLoopCount = 1;

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
    downlink_->socket_.native_non_blocking(false, ec);
    downlink_->socket_.non_blocking(false, ec);
    do_io();
  }

  void close() override {
    VLOG(1) << "Connection (content-provider) " << connection_id()
            << " disconnected";
    asio::error_code ec;
    downlink_->socket_.close(ec);
    on_disconnect();
  }

 private:
  void do_io() {
    done_[0] = false;
    done_[1] = false;

    VLOG(1) << "Connection (content-provider) " << connection_id() << " start to do IO";
    scoped_refptr<ContentProviderConnection> self(this);
    g_in_provider_mutex.lock();

    asio::async_write(downlink_->socket_, const_buffer(g_send_buffer),
      [this, self](asio::error_code ec, size_t bytes_transferred) {
        if (ec.value() == asio::error::bad_descriptor || ec.value() == asio::error::operation_aborted) {
          goto done;
        }
        if (ec || bytes_transferred != g_send_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                       << " Failed to transfer data: " << ec.value();
        } else {
          VLOG(1) << "Connection (content-provider) " << connection_id()
                  << " written: " << bytes_transferred << " bytes";
        }
      done:
        done_[0] = true;
        shutdown(ec);
    });

    asio::async_read(downlink_->socket_, mutable_buffer(*g_recv_buffer),
      [this, self](asio::error_code ec, size_t bytes_transferred) {
        if (ec.value() == asio::error::bad_descriptor || ec.value() == asio::error::operation_aborted) {
          goto done;
        }
        if (ec || bytes_transferred != g_send_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << connection_id()
                       << " Failed to transfer data: " << ec;
        } else {
          VLOG(1) << "Connection (content-provider) " << connection_id()
                  << " read: " << bytes_transferred << " bytes";
        }
        g_recv_buffer->append(bytes_transferred);
      done:
        done_[1] = true;
        shutdown(ec);
    });
  }

  void shutdown(asio::error_code ec) {
    if (!done_[0] || !done_[1]) {
      return;
    }
    g_in_provider_mutex.unlock();
    if (ec) {
      return;
    }
    shutdown_impl();
  }

  void shutdown_impl() {
    if (!g_in_consumer_mutex.try_lock()) {
      scoped_refptr<ContentProviderConnection> self(this);
      asio::post(*io_context_, [this, self]() {
        shutdown_impl();
      });
      return;
    }
    // consumer locked
    do_io();
    g_in_consumer_mutex.unlock();
  }

  bool done_[2] = { false, false };
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

void GenerateConnectRequest(std::string host, int port_num, IOBuf *buf) {
  std::string request_header = absl::StrFormat(
      "CONNECT %s:%d HTTP/1.1\r\n"
      "Host: packages.endpointdev.com:443\r\n"
      "User-Agent: curl/7.77.0\r\n"
      "Proxy-Connection: Keep-Alive\r\n"
      "\r\n", host.c_str(), port_num);
  buf->reserve(request_header.size(), 0);
  memcpy(buf->mutable_buffer(), request_header.c_str(), request_header.size());
  buf->prepend(request_header.size());
}

#define XX(num, name, string) \
struct CryptoTraits##name { \
  static constexpr cipher_method value = CRYPTO_##name; \
};
CIPHER_METHOD_VALID_MAP(XX)
#undef XX

// [content provider] <== [ss server] <== [ss local] <== [content consumer]
template<typename T>
class SsEndToEndBM : public benchmark::Fixture {
 public:
  void SetUp(::benchmark::State& state) override {
    StartWorkThread();
    absl::SetFlag(&FLAGS_method, T::value);
    StartBackgroundTasks();

    GenerateRandContent(state.range(0));
  }

  void StartBackgroundTasks() {
    std::mutex m;
    bool done = false;
    asio::post(io_context_, [this, &m, &done]() {
      std::lock_guard<std::mutex> lk(m);
      auto ec = StartContentProvider(GetReusableEndpoint(), SOMAXCONN);
      if (ec) {
        return;
      }
      ec = StartServer(GetReusableEndpoint(), SOMAXCONN);
      if (ec) {
        return;
      }
      ec = StartLocal(server_endpoint_, GetReusableEndpoint(), SOMAXCONN);
      if (ec) {
        return;
      }
      done = true;
    });
    while (true) {
      std::lock_guard<std::mutex> lk(m);
      if (done)
        break;
    }
  }

  void TearDown(::benchmark::State& state) override {
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

 public:
  void SendRequestAndCheckResponse_Pre(asio::ip::tcp::socket& s) {
    auto endpoint = local_endpoint_;

    asio::error_code ec;
    s.connect(endpoint, ec);
    CHECK(!ec) << "Connection (content-consumer) connect failure " << ec;
    auto request_buf = IOBuf::create(SOCKET_BUF_SIZE);
    GenerateConnectRequest("localhost", content_provider_endpoint_.port(),
                           request_buf.get());

    size_t written = asio::write(s, const_buffer(*request_buf), ec);
    VLOG(1) << "Connection (content-consumer) written: " << written << " bytes";
    CHECK(!ec) << "Connection (content-consumer) write failure " << ec;

    constexpr int response_len = sizeof(kConnectResponse) - 1;
    IOBuf response_buf;
    response_buf.reserve(0, response_len);
    size_t read = asio::read(s, asio::mutable_buffer(response_buf.mutable_tail(), response_len), ec);
    VLOG(1) << "Connection (content-consumer) read: " << read << " bytes";
    response_buf.append(read);
    CHECK_EQ((int)read, response_len) << "Partial read";

#if 0
    const char* buffer = reinterpret_cast<const char*>(response_buf.data());
#endif
    size_t buffer_length = response_buf.length();
    CHECK_EQ((int)buffer_length, response_len) << "Partial read";
  }

  void SendRequestAndCheckResponse(asio::ip::tcp::socket& s, asio::io_context& io_context, benchmark::State& state) {
    asio::error_code ec;

    while (true) {
      std::lock_guard<std::mutex> lk(g_in_consumer_mutex);
      if (!g_in_provider_mutex.try_lock()) {
        break;
      }
      g_in_provider_mutex.unlock();
    }
    std::lock_guard<std::mutex> done_lk(g_in_consumer_mutex);
    auto work_guard = std::make_shared<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

    auto start = std::chrono::high_resolution_clock::now();

    VLOG(1) << "Connection (content-consumer) start to do IO";
    asio::async_write(s, const_buffer(g_send_buffer),
      [work_guard](asio::error_code ec, size_t written) {
      VLOG(1) << "Connection (content-consumer) written: " << written << " bytes";
      CHECK(!ec) << "Connection (content-consumer) write failure " << ec;
      CHECK_EQ(written, g_send_buffer.length()) << "Partial written";
    });

    IOBuf resp_buffer;
    resp_buffer.reserve(0, g_send_buffer.length());
    asio::async_read(s, tail_buffer(resp_buffer),
      [work_guard, &resp_buffer](asio::error_code ec, size_t read) {
      VLOG(1) << "Connection (content-consumer) read: " << read << " bytes";
      resp_buffer.append(read);
      CHECK(!ec) << "Connection (content-consumer) read failure " << ec;
    });
    work_guard.reset();
    io_context.restart();
    io_context.run();

#if 0
    const char* buffer = reinterpret_cast<const char*>(resp_buffer.data());
#endif
    size_t buffer_length = resp_buffer.length();
    CHECK_EQ(buffer_length, g_send_buffer.length()) << "Partial read";

    {
      std::lock_guard<std::mutex> lk(g_in_provider_mutex);
      CHECK_EQ(g_recv_buffer->length(), g_send_buffer.length()) << "Partial read";
      g_recv_buffer->clear();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        end - start);
    state.SetIterationTime(elapsed_seconds.count());
  }

  void SendRequestAndCheckResponse_Post(asio::ip::tcp::socket& s) {
    asio::error_code ec;
    s.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
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
} // namespace

// Register the function as a benchmark

#define XX(num, name, string) \
  BENCHMARK_TEMPLATE_DEFINE_F(SsEndToEndBM, name, CryptoTraits##name)(benchmark::State& state) { \
    asio::io_context io_context;                                        \
    asio::ip::tcp::socket s(io_context);                                \
    SendRequestAndCheckResponse_Pre(s);                                 \
    for (auto _ : state) {                                              \
      SendRequestAndCheckResponse(s, io_context, state);                \
    }                                                                   \
    SendRequestAndCheckResponse_Post(s);                                \
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(state.range(0))); \
  }                                                                     \
  BENCHMARK_REGISTER_F(SsEndToEndBM, name)              \
    ->Name("SsEndToEndBM_FullDuplex_" # name)->Range(4096, 1*1024*1024)->UseManualTime();
CIPHER_METHOD_MAP_SODIUM(XX)
CIPHER_METHOD_MAP_HTTP(XX)
CIPHER_METHOD_MAP_HTTP2(XX)
#undef XX

class ASIOFixture : public benchmark::Fixture {
 public:
  ASIOFixture() : s1(io_context), s2(io_context) {}
  void SetUp(::benchmark::State& state) override {
    asio::error_code ec;
    asio::connect_pipe(s2, s1, ec);
    CHECK(!ec) << "connect_pair failure " << ec;

    GenerateRandContent(state.range(0));
  }

  void TearDown(::benchmark::State& state) override {
    asio::error_code ec;
    s1.close(ec);
    CHECK(!ec) << "close failure " << ec;
    s2.close(ec);
    CHECK(!ec) << "close failure " << ec;
  }

 protected:
  asio::io_context io_context;
  asio::writable_pipe s1;
  asio::readable_pipe s2;
};

BENCHMARK_DEFINE_F(ASIOFixture, PlainIO)(benchmark::State& state)  {
  for (auto _ : state) {
    auto work_guard = std::make_shared<asio::executor_work_guard<asio::io_context::executor_type>>(io_context.get_executor());

    IOBuf req_buffer;
    req_buffer.reserve(0, g_send_buffer.length());
    memcpy(req_buffer.mutable_tail(), g_send_buffer.data(), g_send_buffer.length());
    req_buffer.append(g_send_buffer.length());

    IOBuf resp_buffer;
    resp_buffer.reserve(0, g_send_buffer.length());

    //
    // START
    //
    auto start = std::chrono::high_resolution_clock::now();

    asio::async_write(s1, const_buffer(req_buffer),
      [work_guard](asio::error_code ec, size_t written) {
      CHECK(!ec) << "Connection (content-provider) written failure " << ec;
      VLOG(1) << "Connection (content-provider) written: " << written;
    });

    asio::async_read(s2, mutable_buffer(*g_recv_buffer),
      [work_guard](asio::error_code ec, size_t read) {
      CHECK(!ec) << "Connection (content-provider) read failure " << ec;
      VLOG(1) << "Connection (content-provider) read: " << read;
    });

    work_guard.reset();
    io_context.restart();
    io_context.run();

    //
    // END
    //
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        end - start);
    state.SetIterationTime(elapsed_seconds.count());
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(state.range(0)));
}

BENCHMARK_REGISTER_F(ASIOFixture, PlainIO)->Range(4096, 1*1024*1024)->UseManualTime();

int main(int argc, char** argv) {
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
  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_ipv6_mode, false);

  ::benchmark::Initialize(&argc, argv);
  absl::ParseCommandLine(argc, argv);

#ifdef _WIN32
  int iResult = 0;
  WSADATA wsaData = {0};
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  CHECK_EQ(iResult, 0) << "WSAStartup failure";
#endif
#ifdef _WIN32
  LOG(WARNING) << "Benchmark Started";
#endif

  CRYPTO_library_init();

#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif

  if (absl::GetFlag(FLAGS_ipv6_mode)) {
    CHECK(Net_ipv6works()) << "IPv6 stack is required but not available";
  }

  // avoid triggering flag saver
  absl::SetFlag(&FLAGS_congestion_algorithm, "cubic");
  absl::SetFlag(&FLAGS_password, "<dummy-password>");

  ::benchmark::RunSpecifiedBenchmarks();

  ::benchmark::Shutdown();
  return 0;
}
