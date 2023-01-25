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
#include "cli/socks5_server.hpp"
#include "config/config.hpp"
#include "core/cipher.hpp"
#include "core/iobuf.hpp"
#include "core/rand_util.hpp"
#include "core/ref_counted.hpp"
#include "core/scoped_refptr.hpp"
#include "core/stringprintf.hpp"
#include "server/ss_server.hpp"

#include "test_util.hpp"

namespace {

static IOBuf content_buffer;
static std::mutex recv_mutex;
static std::unique_ptr<IOBuf> recv_content_buffer;
static const char kConnectResponse[] = "HTTP/1.1 200 Connection established\r\n\r\n";
static int kContentMaxSize = 10 * 1024 * 1024;

void GenerateRandContent(int size) {
  content_buffer.clear();
  content_buffer.reserve(0, size);

  RandBytes(content_buffer.mutable_data(), std::min(256, size));
  for (int i = 1; i < size / 256; ++i) {
    memcpy(content_buffer.mutable_data() + 256 * i, content_buffer.data(), 256);
  }
  content_buffer.append(size);

  recv_content_buffer = IOBuf::create(size);
}

class ContentProviderConnection  : public RefCountedThreadSafe<ContentProviderConnection>,
                                   public Connection {
 public:
  ContentProviderConnection(asio::io_context& io_context,
                            const asio::ip::tcp::endpoint& remote_endpoint)
      : Connection(io_context, remote_endpoint) {}

  ~ContentProviderConnection() override {
    VLOG(2) << "Connection (content-provider) " << connection_id() << " freed memory";
  }

  ContentProviderConnection(const ContentProviderConnection&) = delete;
  ContentProviderConnection& operator=(const ContentProviderConnection&) = delete;

  ContentProviderConnection(ContentProviderConnection&&) = delete;
  ContentProviderConnection& operator=(ContentProviderConnection&&) = delete;

  void start() override {
    VLOG(2) << "Connection (content-provider) " << connection_id() << " start to do IO";

    scoped_refptr<ContentProviderConnection> self(this);
    asio::async_write(socket_, const_buffer(content_buffer),
      [self](asio::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred != content_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << self->connection_id()
                       << " Failed to transfer data: " << ec;
        } else {
          VLOG(2) << "Connection (content-provider) " << self->connection_id()
                  << " written: " << bytes_transferred << " bytes";
        }
    });

    recv_mutex.lock();
    asio::async_read(socket_, mutable_buffer(*recv_content_buffer),
      [self](asio::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred != content_buffer.length()) {
          LOG(WARNING) << "Connection (content-provider) " << self->connection_id()
                       << " Failed to transfer data: " << ec;
        } else {
          VLOG(2) << "Connection (content-provider) " << self->connection_id()
                  << " read: " << bytes_transferred << " bytes";
        }
        recv_content_buffer->append(bytes_transferred);
        recv_mutex.unlock();
        DCHECK_EQ(self->socket_.available(), 0u);
        self->socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    });
  }

  void close() override {
    VLOG(2) << "Connection (content-provider) " << connection_id()
            << " disconnected";
    asio::error_code ec;
    socket_.close(ec);
    auto cb = std::move(disconnect_cb_);
    if (cb) {
      cb();
    }
  }
};

class ContentProviderConnectionFactory : public ConnectionFactory {
 public:
   using ConnectionType = ContentProviderConnection;
   scoped_refptr<ConnectionType> Create(asio::io_context& io_context,
                                        const asio::ip::tcp::endpoint& remote_endpoint) {
     return MakeRefCounted<ConnectionType>(io_context, remote_endpoint);
   }
   const char* Name() override { return "content-provider"; };
   const char* ShortName() override { return "cp"; };
};

typedef ContentServer<ContentProviderConnectionFactory> ContentProviderServer;

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

// [content provider] <== [ss server] <== [ss local] <== [content consumer]
class SsEndToEndTest : public ::testing::Test {
 public:
  void SetUp() override {
    StartWorkThread();
    absl::SetFlag(&FLAGS_password, "<dummy-password>");
    std::mutex m;
    bool done = 0;
    io_context_.post([this, &m, &done]() {
      std::lock_guard<std::mutex> lk(m);
      auto ec = StartContentProvider(GetReusableEndpoint(), 1);
      ASSERT_FALSE(ec) << ec;
      ec = StartServer(GetReusableEndpoint(), 1);
      ASSERT_FALSE(ec) << ec;
      ec = StartLocal(server_endpoint_, GetReusableEndpoint(), 1);
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
    auto addr = asio::ip::make_address("127.0.0.1", ec);
    // ASSERT_FALSE(ec) << ec;
    asio::ip::tcp::endpoint endpoint;
    endpoint.address(addr);
    endpoint.port(port_num);
    return endpoint;
  }

  void StartWorkThread() {
    thread_ = std::make_unique<std::thread>([this]() {
      asio::error_code ec;
      VLOG(2) << "background thread started";

      work_guard_ = std::make_unique<asio::io_context::work>(io_context_);
      io_context_.run(ec);
      if (ec) {
        LOG(ERROR) << "io_context failed due to: " << ec;
      }
      io_context_.reset();

      VLOG(2) << "background thread stopped";
    });

    io_context_.post([this]() {
      if (!SetThreadName(thread_->native_handle(), "background")) {
        PLOG(WARNING) << "failed to set thread name";
      }
      if (!SetThreadPriority(thread_->native_handle(), ThreadPriority::ABOVE_NORMAL)) {
        PLOG(WARNING) << "failed to set thread priority";
      }
    });
  }

  void SendRequestAndCheckResponse() {
    asio::io_context io_context;
    asio::ip::tcp::socket s(io_context);

    auto endpoint = local_endpoint_;

    asio::error_code ec;
    s.connect(endpoint, ec);
    ASSERT_FALSE(ec) << ec;
    auto request_buf = IOBuf::create(SOCKET_BUF_SIZE);
    GenerateConnectRequest("127.0.0.1", content_provider_endpoint_.port(),
                           request_buf.get());

    size_t written = asio::write(s, const_buffer(*request_buf), ec);
    VLOG(2) << "Connection (content-consumer) written: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, request_buf->length());

    written = asio::write(s, const_buffer(content_buffer), ec);
    VLOG(2) << "Connection (content-consumer) written: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, content_buffer.length());

    IOBuf response_buf;
    response_buf.reserve(0, kContentMaxSize + 1024);
    size_t read = asio::read(s, tail_buffer(response_buf), ec);
    VLOG(2) << "Connection (content-consumer) read: " << read << " bytes";
    response_buf.append(read);
    *response_buf.mutable_tail() = '\0';
    EXPECT_EQ(ec, asio::error::eof) << ec;

    const char* buffer = reinterpret_cast<const char*>(response_buf.data());
    size_t buffer_length = response_buf.length();
    ASSERT_GE(buffer_length, sizeof(kConnectResponse) - 1);
    ASSERT_THAT(buffer, ::testing::StartsWith(kConnectResponse));
    buffer += sizeof(kConnectResponse) - 1;
    buffer_length -= sizeof(kConnectResponse) - 1;
    ASSERT_EQ(buffer_length, content_buffer.length());
    ASSERT_EQ(::testing::Bytes(buffer, buffer_length),
              ::testing::Bytes(content_buffer.data(), content_buffer.length()));

    std::lock_guard<std::mutex> lk(recv_mutex);
    ASSERT_EQ(recv_content_buffer->length(), content_buffer.length());
    ASSERT_EQ(::testing::Bytes(recv_content_buffer->data(), recv_content_buffer->length()),
              ::testing::Bytes(content_buffer.data(), content_buffer.length()));

    s.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
    EXPECT_FALSE(ec) << ec;
  }

 private:
  asio::error_code StartContentProvider(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    content_provider_server_ = std::make_unique<ContentProviderServer>(io_context_);
    content_provider_server_->listen(endpoint, backlog, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    content_provider_endpoint_ = content_provider_server_->endpoint();
    VLOG(2) << "content provider listening at " << content_provider_endpoint_;

    return ec;
  }

  void StopContentProvider() {
    if (content_provider_server_) {
      content_provider_server_->stop();
    }
  }

  asio::error_code StartServer(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    server_server_ = std::make_unique<SsServer>(io_context_);
    server_server_->listen(endpoint, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    server_endpoint_ = server_server_->endpoint();
    VLOG(2) << "tcp server listening at " << server_endpoint_;

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

    local_server_ = std::make_unique<Socks5Server>(io_context_, remote_endpoint);
    local_server_->listen(endpoint, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      local_server_->stop();
      return ec;
    }

    local_endpoint_ = local_server_->endpoint();
    VLOG(2) << "local server listening at " << local_endpoint_ << " with upstream " << remote_endpoint;

    return ec;
  }

  void StopClient() {
    if (local_server_) {
      local_server_->stop();
    }
  }

 private:
  asio::io_context io_context_;
  std::unique_ptr<asio::io_context::work> work_guard_;
  std::unique_ptr<std::thread> thread_;

  std::unique_ptr<ContentProviderServer> content_provider_server_;
  asio::ip::tcp::endpoint content_provider_endpoint_;
  std::unique_ptr<SsServer> server_server_;
  asio::ip::tcp::endpoint server_endpoint_;
  std::unique_ptr<Socks5Server> local_server_;
  asio::ip::tcp::endpoint local_endpoint_;
};
}

#define XX(num, name, string) \
  TEST_F(SsEndToEndTest, name##_256B) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(256); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_4K) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(4096); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_32K) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(32 * 1024); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_256K) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(256 * 1024); \
    SendRequestAndCheckResponse(); \
  } \
  TEST_F(SsEndToEndTest, name##_1M) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(1024 * 1024); \
    SendRequestAndCheckResponse(); \
  }
CIPHER_METHOD_OLD_MAP(XX)
#undef XX

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_tcp_user_timeout, 1000);

  ::CRYPTO_library_init();

  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);

  return RUN_ALL_TESTS();
}
