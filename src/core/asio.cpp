// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "core/asio.hpp"

#ifdef _WIN32
#include <wincrypt.h>
#elif defined(__APPLE__)
#include <Security/Security.h>
#endif


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
// new in GCC 7, see
// https://developers.redhat.com/blog/2017/03/10/wimplicit-fallthrough-in-gcc-7
#if (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif  // (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)

#include "asio/impl/src.hpp"
#include "asio/ssl/impl/src.hpp"

#pragma GCC diagnostic pop

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
  return o << ec.message();
}

void load_ca_to_ssl_ctx(asio::ssl::context &ssl_ctx) {
#ifdef _WIN32
  HCERTSTORE cert_store = NULL;
  asio::error_code ec;
  PCCERT_CONTEXT cert = nullptr;

  cert_store = CertOpenSystemStoreW(NULL, L"ROOT");
  if (!cert_store) {
    PLOG(WARNING) << "CertOpenSystemStoreW failed";
    goto out;
  }

  while((cert = CertEnumCertificatesInStore(cert_store, cert))) {
    const char* data = (const char *)cert->pbCertEncoded;
    int len = cert->cbCertEncoded;
    bssl::UniquePtr<X509> cert(d2i_X509(nullptr, (const unsigned char**)&data, len));
    if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
        X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
      ssl_ctx.add_certificate_authority(asio::const_buffer(data, len), ec);
      char buf[4096] = {};
      VLOG(2) << "Loading ca: " << X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));
    }
  }

out:
  if (cert_store) {
    CertCloseStore(cert_store, CERT_CLOSE_STORE_FORCE_FLAG);
  }
#elif defined(__APPLE__)
  SecTrustSettingsDomain domain = kSecTrustSettingsDomainSystem;
  CFArrayRef certs;
  OSStatus status;
  asio::error_code ec;
  CFIndex size;
  status = SecTrustSettingsCopyCertificates(domain, &certs);
  if (status != errSecSuccess) {
    goto out;
  }

  size = CFArrayGetCount(certs);
  for (CFIndex i = 0; i < size; ++i) {
    SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, i);
    // TODO copy if trusted
    CFDataRef data_ref;
    data_ref = SecCertificateCopyData(cert);

    if (data_ref == NULL) {
      LOG(WARNING) << "Empty data from Security framework";
      goto out;
    } else {
      const char* data = (const char *)CFDataGetBytePtr(data_ref);
      int len = CFDataGetLength(data_ref);
      bssl::UniquePtr<X509> cert(d2i_X509(nullptr, (const unsigned char**)&data, len));
      if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
          X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
        ssl_ctx.add_certificate_authority(asio::const_buffer(data, len), ec);
        char buf[4096] = {};
        VLOG(2) << "Loading ca: " << X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));
      }
      CFRelease(data_ref);
    }
  }
out:
  CFRelease(certs);
#else
  // cert list copied from golang src/crypto/x509/root_unix.go
  const char *ca_bundle_paths[] = {
    "/etc/ssl/certs/ca-certificates.crt",     // Debian/Ubuntu/Gentoo etc.
    "/etc/pki/tls/certs/ca-bundle.crt",       // Fedora/RHEL
    "/etc/ssl/ca-bundle.pem",                 // OpenSUSE
    "/etc/openssl/certs/ca-certificates.crt", // NetBSD
    "/etc/ssl/cert.pem",                      // OpenBSD
    "/usr/local/share/certs/ca-root-nss.crt", // FreeBSD/DragonFly
    "/etc/pki/tls/cacert.pem",                // OpenELEC
    "/etc/certs/ca-certificates.crt",         // Solaris 11.2+
  };
  asio::error_code ec;
  for (auto ca_bundle : ca_bundle_paths) {
    ssl_ctx.load_verify_file(ca_bundle, ec);
    if (!ec) {
      VLOG(2) << "Loading ca bundle: " << ca_bundle);
    }
  }
#endif
}
