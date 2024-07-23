// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

#include <build/build_config.h>
#include <gtest/gtest.h>

#include "core/utils.hpp"
#include "net/asio_ssl_internal.hpp"

#ifdef HAVE_BUILTIN_CA_BUNDLE_CRT
TEST(SSL_TEST, LoadBuiltinCaBundle) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx;
  ssl_ctx.reset(::SSL_CTX_new(::TLS_client_method()));
  std::string_view ca_bundle_content(_binary_ca_bundle_crt_start,
                                     _binary_ca_bundle_crt_end - _binary_ca_bundle_crt_start);
  ASSERT_FALSE(ca_bundle_content.empty());
  int result = load_ca_to_ssl_ctx_from_mem(ssl_ctx.get(), ca_bundle_content);
  ASSERT_NE(result, 0);
}
#endif

TEST(SSL_TEST, LoadSupplementaryCaBundle) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx;
  ssl_ctx.reset(::SSL_CTX_new(::TLS_client_method()));
  std::string_view ca_content(_binary_supplementary_ca_bundle_crt_start,
                              _binary_supplementary_ca_bundle_crt_end - _binary_supplementary_ca_bundle_crt_start);
  ASSERT_FALSE(ca_content.empty());
  int result = load_ca_to_ssl_ctx_from_mem(ssl_ctx.get(), ca_content);
  ASSERT_NE(result, 0);
}

TEST(SSL_TEST, LoadSystemCa) {
  bssl::UniquePtr<SSL_CTX> ssl_ctx;
  ssl_ctx.reset(::SSL_CTX_new(::TLS_client_method()));
  int result = load_ca_to_ssl_ctx_system(ssl_ctx.get());
#ifdef _WIN32
  if (IsWindowsVersionBNOrGreater(6, 3, 0)) {
    ASSERT_NE(result, 0);
  } else {
    GTEST_SKIP() << "skipped as system version is too low";
  }
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OHOS) || BUILDFLAG(IS_FREEBSD) || \
    BUILDFLAG(IS_LINUX) || !defined(HAVE_BUILTIN_CA_BUNDLE_CRT)
  ASSERT_NE(result, 0);
#else
  // we don't test on iOS
  GTEST_SKIP() << "skipped as system is not supported";
#endif
}
