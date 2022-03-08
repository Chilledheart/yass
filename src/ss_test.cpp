// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include <gmock/gmock.h>

#include <absl/debugging/failure_signal_handler.h>
#include <absl/debugging/symbolize.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <openssl/crypto.h>

#include "cli/socks5_factory.hpp"
#include "config/config.hpp"
#include "core/cipher.hpp"
#include "core/iobuf.hpp"
#include "core/rand_util.hpp"
#include "core/stringprintf.hpp"
#include "server/ss_factory.hpp"

#include "test_util.hpp"

using ::testing::StartsWith;

namespace {

static IOBuf content_buffer;
static const char kConnectResponse[] = "HTTP/1.1 200 Connection established\r\n\r\n";
static int kContentMaxSize = 1024 * 1024;

void GenerateRandContent(int max = kContentMaxSize) {
  int content_size = max;
  content_buffer.clear();
  content_buffer.reserve(0, content_size);
  RandBytes(content_buffer.mutable_data(), std::min(256, content_size));
  content_buffer.append(content_size);
}

class ContentProviderConnection : public Connection {
 public:
  ContentProviderConnection(asio::io_context& io_context,
                            const asio::ip::tcp::endpoint& remote_endpoint)
      : Connection(io_context, remote_endpoint) {}

  ~ContentProviderConnection() {
    asio::error_code ec;
    socket_.close(ec);
  }

  void start() override {
    asio::async_write(socket_, const_buffer(content_buffer),
      [this](asio::error_code ec, size_t bytes_transferred) {
        if (ec || bytes_transferred != content_buffer.length()) {
          LOG(WARNING) << "Failed to transfer data: " << ec;
          socket_.close(ec);
        } else {
          VLOG(2) << "content provider: written: " << bytes_transferred << " bytes";
          socket_.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
        }
    });
  }

  void close() override {
    asio::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
  }
};

typedef ServiceFactory<ContentProviderConnection> CpFactory;

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

asio::error_code ReadUntilEof(asio::ip::tcp::socket *s, IOBuf* buf) {
  asio::error_code ec;
  buf->reserve(0, kContentMaxSize * 1024);
  while (buf->tailroom()) {
    size_t read = s->read_some(tail_buffer(*buf), ec);
    if (ec == asio::error::eof) {
      VLOG(2) << "content consumer: eof";
      ec = asio::error_code();
      break;
    }
    if (ec) {
      break;
    }
    VLOG(3) << "content consumer: read: " << read << " bytes";
    buf->append(read);
  }
  return ec;
}

// [content provider] <== [ss server] <== [ss local] <== [content consumer]
class SsEndToEndTest : public ::testing::Test {
 public:
  void SetUp() override {
    absl::SetFlag(&FLAGS_password, "<dummy-password>");
    auto ec = StartContentProvider(GetContentProviderEndpoint(), 1);
    ASSERT_FALSE(ec) << ec;
    ec = StartServer(GetServerEndpoint(), 1);
    ASSERT_FALSE(ec) << ec;
    ec = StartLocal(GetServerEndpoint(), GetLocalEndpoint(), 1);
    ASSERT_FALSE(ec) << ec;
  }

  void TearDown() override {
    StopClient();
    StopServer();
    StopContentProvider();
  }

 protected:
  asio::ip::tcp::endpoint GetContentProviderEndpoint() const {
    return GetEndpoint(9001);
  }

  asio::ip::tcp::endpoint GetServerEndpoint() const {
    return GetEndpoint(9002);
  }

  asio::ip::tcp::endpoint GetLocalEndpoint() const {
    return GetEndpoint(9003);
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
    content_provider_io_thread_ = std::make_unique<std::thread>([this]() {
      VLOG(2) << "content provider thread started";
      asio::error_code ec;
      content_provider_io_context_.run(ec);
      if (ec) {
        LOG(ERROR) << "content provider failed due to: " << ec;
      }
      VLOG(2) << "content provider thread ended";
    });
    server_io_thread_ = std::make_unique<std::thread>([this]() {
      VLOG(2) << "server thread started";
      asio::error_code ec;
      server_io_context_.run(ec);
      if (ec) {
        LOG(ERROR) << "remote server failed due to: " << ec;
      }
      VLOG(2) << "server thread ended";
    });
    local_io_thread_ = std::make_unique<std::thread>([this]() {
      VLOG(2) << "local thread started";
      asio::error_code ec;
      local_io_context_.run(ec);
      if (ec) {
        LOG(ERROR) << "local server failed due to: " << ec;
      }
      VLOG(2) << "local thread ended";
    });
  }

  void SendRequestAndCheckResponse(const char* request_data,
                                   size_t request_data_size) {
    StartWorkThread();
    asio::io_context io_context;
    asio::ip::tcp::socket s(io_context);

    auto endpoint = GetLocalEndpoint();

    asio::error_code ec;
    s.connect(endpoint, ec);
    ASSERT_FALSE(ec) << ec;
    auto request_buf = IOBuf::copyBuffer(request_data, request_data_size);
    GenerateConnectRequest("127.0.0.1", GetContentProviderEndpoint().port(),
                           request_buf.get());

    size_t written = asio::write(s, const_buffer(*request_buf), ec);
    VLOG(2) << "content consumer: written: " << written << " bytes";
    EXPECT_FALSE(ec) << ec;
    EXPECT_EQ(written, request_buf->length());

    IOBuf response_buf;
    ec = ReadUntilEof(&s, &response_buf);
    EXPECT_FALSE(ec) << ec;

    const char* buffer = reinterpret_cast<const char*>(response_buf.data());
    size_t buffer_length = response_buf.length();
    ASSERT_GE(buffer_length, sizeof(kConnectResponse) - 1);
    ASSERT_THAT(buffer, StartsWith(kConnectResponse));
    buffer += sizeof(kConnectResponse) - 1;
    buffer_length -= sizeof(kConnectResponse) - 1;
    ASSERT_EQ(buffer_length, content_buffer.length());
    ASSERT_EQ(Bytes(buffer, buffer_length),
              Bytes(content_buffer.data(), content_buffer.length()));
  }

 private:
  asio::error_code StartContentProvider(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    VLOG(2) << "content provider listening at " << endpoint;

    // not used
    asio::ip::tcp::endpoint remote_endpoint;
    content_provider_work_guard_ = std::make_unique<asio::io_context::work>(content_provider_io_context_);
    content_provider_factory_ = std::make_unique<CpFactory>(remote_endpoint);
    content_provider_factory_->listen(endpoint, backlog, ec);
    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }
    return ec;
  }

  void StopContentProvider() {
    if (content_provider_factory_) {
      content_provider_factory_->stop();
      content_provider_factory_->join();
    }
    content_provider_work_guard_.reset();
    if (content_provider_io_thread_) {
      content_provider_io_thread_->join();
      content_provider_io_thread_.reset();
    }
    content_provider_factory_.reset();
  }

  asio::error_code StartServer(asio::ip::tcp::endpoint endpoint, int backlog) {
    asio::error_code ec;

    VLOG(2) << "tcp server listening at " << endpoint;

    // not used
    asio::ip::tcp::endpoint remote_endpoint;
    server_work_guard_ = std::make_unique<asio::io_context::work>(server_io_context_);
    server_factory_ = std::make_unique<SsFactory>(remote_endpoint);
    server_factory_->listen(endpoint, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      return ec;
    }

    return ec;
  }

  void StopServer() {
    if (server_factory_) {
      server_factory_->stop();
      server_factory_->join();
    }
    server_work_guard_.reset();
    if (server_io_thread_) {
      server_io_thread_->join();
      server_io_thread_.reset();
    }
    server_factory_.reset();
  }

  asio::error_code StartLocal(asio::ip::tcp::endpoint remote_endpoint,
                              asio::ip::tcp::endpoint endpoint,
                              int backlog) {
    asio::error_code ec;

    VLOG(2) << "local server listening at " << endpoint << " with upstream " << remote_endpoint;

    local_work_guard_ = std::make_unique<asio::io_context::work>(local_io_context_);
    local_factory_ = std::make_unique<Socks5Factory>(remote_endpoint);
    local_factory_->listen(endpoint, backlog, ec);

    if (ec) {
      LOG(ERROR) << "listen failed due to: " << ec;
      local_factory_->stop();
      local_factory_->join();
      local_work_guard_.reset();
      return ec;
    }

    return ec;
  }

  void StopClient() {
    if (local_factory_) {
      local_factory_->stop();
      local_factory_->join();
    }
    local_work_guard_.reset();
    if (local_io_thread_) {
      local_io_thread_->join();
      local_io_thread_.reset();
    }
    local_factory_.reset();
  }

 private:
  asio::io_context content_provider_io_context_;
  std::unique_ptr<asio::io_context::work> content_provider_work_guard_;
  std::unique_ptr<std::thread> content_provider_io_thread_;
  std::unique_ptr<CpFactory> content_provider_factory_;

  asio::io_context server_io_context_;
  std::unique_ptr<asio::io_context::work> server_work_guard_;
  std::unique_ptr<std::thread> server_io_thread_;
  std::unique_ptr<SsFactory> server_factory_;

  asio::io_context local_io_context_;
  std::unique_ptr<asio::io_context::work> local_work_guard_;
  std::unique_ptr<std::thread> local_io_thread_;
  std::unique_ptr<Socks5Factory> local_factory_;
};
}

#define XX(num, name, string) \
  TEST_F(SsEndToEndTest, name##_256B) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(256); \
    SendRequestAndCheckResponse("DUMMY REQUEST", sizeof("DUMMY REQUEST")); \
  } \
  TEST_F(SsEndToEndTest, name##_4K) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(4096); \
    SendRequestAndCheckResponse("DUMMY REQUEST", sizeof("DUMMY REQUEST")); \
  } \
  TEST_F(SsEndToEndTest, name##_256K) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(256 * 1024); \
    SendRequestAndCheckResponse("DUMMY REQUEST", sizeof("DUMMY REQUEST")); \
  } \
  TEST_F(SsEndToEndTest, name##_1M) { \
    absl::SetFlag(&FLAGS_cipher_method, CRYPTO_##name); \
    GenerateRandContent(1024 * 1024); \
    SendRequestAndCheckResponse("DUMMY REQUEST", sizeof("DUMMY REQUEST")); \
  }
CIPHER_METHOD_VALID_MAP(XX)
#undef XX

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions failure_handle_options;
  absl::InstallFailureSignalHandler(failure_handle_options);

  absl::SetFlag(&FLAGS_log_thread_id, 1);
  absl::SetFlag(&FLAGS_threads, 1);
  ::CRYPTO_library_init();

  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);

  return RUN_ALL_TESTS();
}
