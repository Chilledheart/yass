// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "core/asio.hpp"

#include "core/utils.hpp"
#include "config/config.hpp"

#include <absl/strings/str_split.h>
#include <string>

#ifdef _WIN32
#if _WIN32_WINNT >= 0x0602 && !defined(__MINGW32__)
#include <pathcch.h>
#else
struct IUnknown;
#include <shlwapi.h>
#endif
#include <wincrypt.h>
#undef X509_NAME
#elif defined(__APPLE__)
#include <Security/Security.h>
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wunused-result"
// new in GCC 7, see
// https://developers.redhat.com/blog/2017/03/10/wimplicit-fallthrough-in-gcc-7
#if (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif  // (defined(__GNUC__) && (__GNUC__ >= 7)) || defined(__clang__)

#include "asio/impl/src.hpp"
#include "asio/ssl/impl/src.hpp"

#pragma GCC diagnostic pop

#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
extern "C" const char _binary_ca_bundle_crt_start[];
extern "C" const char _binary_ca_bundle_crt_end[];

// Use internal ca-bundle.crt if necessary
// we take care of the ca-bundle if windows version is below windows 8.1
#if defined(_WIN32) && _WIN32_WINNT < 0x0603
ABSL_FLAG(bool, use_ca_bundle_crt, !IsWindowsVersionBNOrGreater(6, 3, 0), "(TLS) Use internal yass-ca-bundle.crt.");
#else
ABSL_FLAG(bool, use_ca_bundle_crt, false, "(TLS) Use internal yass-ca-bundle.crt.");
#endif

#endif

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
  return o << ec.message();
}

static void print_openssl_error() {
  const char* file;
  int line;
  while (uint32_t error = ERR_get_error_line(&file, &line)) {
    char buf[120];
    ERR_error_string_n(error, buf, sizeof(buf));
    LogMessage(file, line, LOGGING_ERROR).stream()
      << "OpenSSL error: " << buf;
  }
}

#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
static bool load_ca_to_x509_trust(X509_STORE* store, const uint8_t *data, size_t len) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
  BIO_write(bio.get(), data, len);
  bssl::UniquePtr<X509> cert(PEM_read_bio_X509(bio.get(), nullptr, 0, nullptr));
  if (!cert) {
    print_openssl_error();
    return false;
  }
  if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
      X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
    char buf[4096] = {};
    const char* const subject_name = X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));

    if (X509_STORE_add_cert(store, cert.get()) == 1) {
      VLOG(2) << "Loading ca: " << subject_name;
      return true;
    } else {
      unsigned long err = ERR_get_error();
      char buf[120];
      ERR_error_string_n(err, buf, sizeof(buf));
      LOG(WARNING) << "Loading ca failure: " << buf << " at "<< subject_name;
    }
  }
  return false;
}

static const char kEndCertificateMark[] = "-----END CERTIFICATE-----\n";
static void load_ca_to_ssl_ctx_from_mem(SSL_CTX* ssl_ctx, const absl::string_view& cadata) {
  X509_STORE* store = nullptr;
  int count = 0;
  store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!store) {
    LOG(WARNING) << "Can't get SSL CTX cert store";
    goto out;
  }
  for (size_t pos = 0, end = pos; end < cadata.size(); pos = end) {
    end = cadata.find(kEndCertificateMark, pos);
    if (end == absl::string_view::npos) {
      break;
    }
    end += sizeof(kEndCertificateMark) -1;

    absl::string_view cacert(cadata.data() + pos, end - pos);
    if (load_ca_to_x509_trust(store, (const uint8_t*)cacert.data(), cacert.size())) {
      ++count;
    }
  }

out:
  VLOG(1) << "Loading ca from memory: " << count << " certificates";
}
#endif

static bool load_ca_to_ssl_ctx_override(SSL_CTX* ssl_ctx) {
#if defined(_WIN32) || defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
  if (absl::GetFlag(FLAGS_cacert).empty() && absl::GetFlag(FLAGS_use_ca_bundle_crt)) {
    absl::string_view ca_bundle_content(_binary_ca_bundle_crt_start, _binary_ca_bundle_crt_end - _binary_ca_bundle_crt_start);
    load_ca_to_ssl_ctx_from_mem(ssl_ctx, ca_bundle_content);
    return true;
  }
#endif
  std::string ca_bundle = absl::GetFlag(FLAGS_cacert);
  if (!ca_bundle.empty()) {
    int result = SSL_CTX_load_verify_locations(ssl_ctx, ca_bundle.c_str(), nullptr);
    if (result == 1) {
      VLOG(1) << "Loading ca bundle: " << ca_bundle;
    } else {
      print_openssl_error();
    }
    return true;
  }
  std::string ca_path = absl::GetFlag(FLAGS_capath);
  if (!ca_path.empty()) {
    int result = SSL_CTX_load_verify_locations(ssl_ctx, nullptr, ca_path.c_str());
    if (result == 1) {
      VLOG(1) << "Loading ca path: " << ca_bundle;
    } else {
      print_openssl_error();
    }
    return true;
  }
#ifdef _WIN32
#define CA_BUNDLE "yass-ca-bundle.crt"
  // The windows version will automatically look for a CA certs file named 'ca-bundle.crt',
  // either in the same directory as yass.exe, or in the Current Working Directory, or in any folder along your PATH.

  std::vector<std::string> ca_bundles;

  // executable directory
#if _WIN32_WINNT >= 0x0602 && !defined(__MINGW32__)
  std::wstring wexe_path;
  CHECK(GetExecutablePathW(&wexe_path));
  wchar_t wexe_dir[_MAX_PATH+1];
  memcpy(wexe_dir, wexe_path.c_str(), sizeof(wchar_t) *(wexe_path.size() + 1));
  PathCchRemoveFileSpec(wexe_dir, _MAX_PATH);
  std::string exe_dir = SysWideToUTF8(wexe_dir);
#else
  std::string exe_path;
  CHECK(GetExecutablePath(&exe_path));
  char exe_dir[_MAX_PATH+1];
  memcpy(exe_dir, exe_path.c_str(), exe_path.size() + 1);
  PathRemoveFileSpecA(exe_dir);
#endif
  ca_bundles.push_back(std::string(exe_dir) + "\\" + CA_BUNDLE);

  // current directory
  ca_bundles.push_back(CA_BUNDLE);

  // path directory
  std::string path = getenv("PATH");
  std::vector<std::string> paths = absl::StrSplit(path, ';');
  for (const auto& path : paths) {
    ca_bundles.push_back(path + "\\" + CA_BUNDLE);
  }

  for (const auto &ca_bundle : ca_bundles) {
    int result = SSL_CTX_load_verify_locations(ssl_ctx, ca_bundle.c_str(), nullptr);
    if (result == 1) {
      VLOG(1) << "Loading ca bundle: " << ca_bundle;
      return true;
    }
  }
#endif

  return false;
}

void load_ca_to_ssl_ctx(SSL_CTX* ssl_ctx) {
  if (load_ca_to_ssl_ctx_override(ssl_ctx)) {
    return;
  }
#ifdef _WIN32
  HCERTSTORE cert_store = NULL;
  asio::error_code ec;
  PCCERT_CONTEXT cert = nullptr;
  X509_STORE* store = nullptr;
  int count = 0;

  cert_store = CertOpenSystemStoreW(0, L"ROOT");
  if (!cert_store) {
    PLOG(WARNING) << "CertOpenSystemStoreW failed";
    goto out;
  }

  store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!store) {
    LOG(WARNING) << "Can't get SSL CTX cert store";
    goto out;
  }

  while((cert = CertEnumCertificatesInStore(cert_store, cert))) {
    const char* data = (const char *)cert->pbCertEncoded;
    size_t len = cert->cbCertEncoded;
    bssl::UniquePtr<X509> cert(d2i_X509(nullptr, (const unsigned char**)&data, len));
    if (!cert) {
      print_openssl_error();
      continue;
    }
    if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
        X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
      char buf[4096] = {};
      const char* const subject_name = X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));

      bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
      PEM_write_bio_X509(bio.get(), cert.get());
      int ret = BIO_mem_contents(bio.get(), reinterpret_cast<const uint8_t**>(&data), &len);
      if (ret == 0) {
        LOG(WARNING) << "Loading ca failure: Internal Error at "<< subject_name;
        continue;
      }
      if (X509_STORE_add_cert(store, cert.get()) == 1) {
        VLOG(2) << "Loading ca: " << subject_name;
        ++count;
      } else {
        unsigned long err = ERR_get_error();
        char buf[120];
        ERR_error_string_n(err, buf, sizeof(buf));
        LOG(WARNING) << "Loading ca failure: " << buf << " at "<< subject_name;
        continue;
      }
    }
  }

out:
  if (cert_store) {
    CertCloseStore(cert_store, CERT_CLOSE_STORE_FORCE_FLAG);
  }
  VLOG(1) << "Loading ca from SChannel: " << count << " certificates";
#elif defined(__APPLE__)
  SecTrustSettingsDomain domain = kSecTrustSettingsDomainSystem;
  CFArrayRef certs;
  OSStatus status;
  asio::error_code ec;
  CFIndex size;
  X509_STORE* store = nullptr;
  int count = 0;

  status = SecTrustSettingsCopyCertificates(domain, &certs);
  if (status != errSecSuccess) {
    goto out;
  }

  store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!store) {
    LOG(WARNING) << "Can't get SSL CTX cert store";
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
      CFIndex len = CFDataGetLength(data_ref);
      bssl::UniquePtr<X509> cert(d2i_X509(nullptr, (const unsigned char**)&data, len));
      if (!cert) {
        print_openssl_error();
        continue;
      }
      if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
          X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
        char buf[4096] = {};
        const char* const subject_name = X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));

        if (X509_STORE_add_cert(store, cert.get()) == 1) {
          ++count;
          VLOG(2) << "Loading ca: " << subject_name;
        } else {
          unsigned long err = ERR_get_error();
          char buf[120];
          ERR_error_string_n(err, buf, sizeof(buf));
          LOG(WARNING) << "Loading ca failure: " << buf << " at "<< subject_name;
          continue;
        }
      }
      CFRelease(data_ref);
    }
  }
out:
  CFRelease(certs);
  VLOG(1) << "Loading ca from Sec: " << count << " certificates";
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
    int result = SSL_CTX_load_verify_locations(ssl_ctx, ca_bundle, nullptr);
    if (result == 1) {
      VLOG(1) << "Loading ca bundle: " << ca_bundle;
    }
  }
#endif
}
