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

// openssl req -newkey rsa:2048 -keyout pkey.pem -x509 -out cert.crt -days 3650 -nodes -subj /C=XX
static const char kCertificate[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIC+zCCAeOgAwIBAgIUC5yMAfaagwJ/fB7tBADqcYHP3jkwDQYJKoZIhvcNAQEL\n"
"BQAwDTELMAkGA1UEBhMCWFgwHhcNMjMwMTI3MTAzNTIxWhcNMzMwMTI0MTAzNTIx\n"
"WjANMQswCQYDVQQGEwJYWDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
"AKhVtJCRsJBjhWZa6jtMacynN3YrgU2xF8P0eLLEeN0RtymbwtVPIQ2853oj+Uvu\n"
"e8rsXoaW+M8m2MPd8b/by8si/d35KDDi0rijnYrsqjCvYkCotu8mta+VnnM51ueb\n"
"Tr2P4vDT8J3UKGT4Rg9x3xcJx1IiFy5UoruI9XZnnX+pGmo0GjU704B5mLT2eDvo\n"
"BMOMW4YKENVZX5XGpRBj+ufReQBfnZE2cyhbnJUS2fXqxmT+J+988kjQnCX8Rzre\n"
"W5JkngHkhIWV4e38rPg9GuNiiIe6TD12ObMPhfccj5cuPU6wS2bPcBrnFDKpL1Vt\n"
"X98LbYcc3pLjmS6lut9aA8ECAwEAAaNTMFEwHQYDVR0OBBYEFGgeu0k5L1khUQyt\n"
"7UkZNbgS1Q2JMB8GA1UdIwQYMBaAFGgeu0k5L1khUQyt7UkZNbgS1Q2JMA8GA1Ud\n"
"EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAKSYvc/tBjY4XyYu5TM5MPcC\n"
"nIQsHSfjT5jlkq42nhNgP0SWcCOLep0m0GqoERY2A1QWep4ajFdkIhwZGWQOP5RG\n"
"9eACGPIk7Jay1NA+8ZWxpZHfCfUdEM5MmYJHhNc2t4ImMqdBNBdjBUIerLHho0r/\n"
"1mpQghxlFq1ru4yV2mNxFLFOn+CjL+a8n68676lIBo/wUZx8IbX1s5nd7+o9eWMx\n"
"mF1/g9Yljw91+Dj0LJfMtHYOX8kVcWn/ZY1wRFhCLe55pef/W4u8no6eiz/tmQAH\n"
"j/2WPF2h7TQBV9c+9COb56E5GvRcSR2LI5JTUw5BVW43k25dgRaGET61Itn03vE=\n"
"-----END CERTIFICATE-----";
static const char kPrivateKey[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCoVbSQkbCQY4Vm\n"
"Wuo7TGnMpzd2K4FNsRfD9HiyxHjdEbcpm8LVTyENvOd6I/lL7nvK7F6GlvjPJtjD\n"
"3fG/28vLIv3d+Sgw4tK4o52K7Kowr2JAqLbvJrWvlZ5zOdbnm069j+Lw0/Cd1Chk\n"
"+EYPcd8XCcdSIhcuVKK7iPV2Z51/qRpqNBo1O9OAeZi09ng76ATDjFuGChDVWV+V\n"
"xqUQY/rn0XkAX52RNnMoW5yVEtn16sZk/ifvfPJI0Jwl/Ec63luSZJ4B5ISFleHt\n"
"/Kz4PRrjYoiHukw9djmzD4X3HI+XLj1OsEtmz3Aa5xQyqS9VbV/fC22HHN6S45ku\n"
"pbrfWgPBAgMBAAECggEBAJ+XabHqPgAWKmHo7cq8Xk3ldsJ06ojyzbo867VoacIF\n"
"SqaLAsNi2s6AeuCkfHSNrBWt1Mw7E7apeLbxk4G261YyXYb18jGuyeK9U95jE9NG\n"
"Y5szmQPQqk3GRsutWV6JMrSrVpfGB4hKnOVlMF7yMXRRFAR9R4boPMQZS8Yu4/Yj\n"
"nalS88WeSYdm1STuswyHmAv28snV5q9Vf0jBQPgArbLSXFXKTYvJrsi13f9VJ5uW\n"
"tkiejB7RDa2rL7OAuYECVFfEQnDAvmfR4QhfOHQSRyr4w3jTUlK+xpXGBnXzqrJE\n"
"VrIyJGeqk5sQXfiSFhrusTRJBQSwU8bdXCZmetWQ5uUCgYEA1IjBKhhwMjM8HcUN\n"
"/Ho3cKbmkLn2DqioJFPEI0patlYvqmUBUDklGETNOXWwV4FiXLA6rT8l60tptsw3\n"
"W6zwWNkBRxciMbq4S6h24AETUWFqqsrnbNw51TNRSBkrEvy8xBpj/B8f9KmD5LVI\n"
"OeLYOLnP2msxSi8PK0hg2/gCNasCgYEAysLjEILm3jpyS3Pvr4N2xRYa0rvK1Ora\n"
"4DeN/S45mVThKrBBT6FehRDRZjQkR0clydIUV98nIuFuNS00FTs0ha4VeJuY1zR0\n"
"7B1h99GDlfV7dY5aWAwz8/uD7oU244N+XqwSofs5FlnwhTb7GtR8zVAdYW/s6aTr\n"
"iQguA9lp6EMCgYEAhrZ32XrMAsW+4Q+6IcJFyb3AfxOgBwKYMQ53T/cdMF3IsLR8\n"
"9KCEBrH1cupJ7+0ur5l0V8OjAVU3mIowvIcNgQNrb+gV4Hd9wVbyomGMIRUiS0d5\n"
"EOM2NRDmAFEToGFaNOKVZYVE+AtKcnkFYsuKScpdGRDAmUji0Ih7/HFi1SkCgYAZ\n"
"INH3J+HoxKGJjFK2E7rSbgzg9PkMLhb2Fqx4JhRpVkWZfsJ5Vexa3Vy2J9wfIUgj\n"
"nO98fGFjR0DbQkDkKLQ3pP1wNwhYE14yLOoJRmPiX8vvI7c6ljiSEiellcjZpWAx\n"
"521fuby3cmoGeGviRVc6MqWRf8eCpTezgdoCDB299QKBgCdMCtpn86QFFCI1T3n8\n"
"fCHjsb0XWHZTQIQJCItLRbR7hSxQYOO36hc845X9PPdwxn9sxSioKWpivJiy1dyq\n"
"+AdHxnkxe27osq3pwyLvFhBQx2EDBNQXpfm1Os0rmkjS1KrFWrjHGONDS86oCSsY\n"
"N5GKmyTG29qPeKWleARxMACJ\n"
"-----END PRIVATE KEY-----"
;

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
                            const asio::ip::tcp::endpoint& remote_endpoint,
                            bool enable_tls)
      : Connection(io_context, remote_endpoint, false) {}

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
                                        const asio::ip::tcp::endpoint& remote_endpoint,
                                        bool enable_tls) {
     return MakeRefCounted<ConnectionType>(io_context, remote_endpoint, enable_tls);
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

    server_server_ = std::make_unique<SsServer>(io_context_,
                                                asio::ip::tcp::endpoint(),
                                                std::string(),
                                                std::string(kCertificate),
                                                std::string(kPrivateKey));
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

    local_server_ = std::make_unique<Socks5Server>(io_context_, remote_endpoint,
                                                   kCertificate);
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
CIPHER_METHOD_VALID_MAP(XX)
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
