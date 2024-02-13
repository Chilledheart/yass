// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#include "net/asio.hpp"

#ifdef _WIN32
#include "core/windows/dirent.h"
#else
#include <dirent.h>
#endif

#include <filesystem>
#include <string>
#include <absl/strings/str_split.h>
#include <base/files/platform_file.h>
#include <base/files/memory_mapped_file.h>

#include "core/utils.hpp"
#include "config/config.hpp"
#include "net/x509_util.hpp"

#ifdef _WIN32
#include <wincrypt.h>
#undef X509_NAME
#elif BUILDFLAG(IS_MAC)
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

#ifdef HAVE_BUILTIN_CA_BUNDLE_CRT

extern "C" const char _binary_ca_bundle_crt_start[];
extern "C" const char _binary_ca_bundle_crt_end[];

// Use internal ca-bundle.crt if necessary
// we take care of the ca-bundle if windows version is below windows 8.1
ABSL_FLAG(bool, use_ca_bundle_crt,
#if defined(_WIN32) && _WIN32_WINNT < 0x0603
          !IsWindowsVersionBNOrGreater(6, 3, 0),
#elif defined(__FreeBSD__)
          true,
#else
          false,
#endif
          "Use internal ca-bundle.crt instead of system CA store.");

#endif // HAVE_BUILTIN_CA_BUNDLE_CRT

std::ostream& operator<<(std::ostream& o, asio::error_code ec) {
#ifdef _WIN32
  return o << ec.message() << " value: " << ec.value();
#else
  return o << ec.message();
#endif
}

static void print_openssl_error() {
  const char* file;
  int line;
  while (uint32_t error = ERR_get_error_line(&file, &line)) {
    char buf[120];
    ERR_error_string_n(error, buf, sizeof(buf));
    ::yass::LogMessage(file, line, LOGGING_ERROR).stream()
      << "OpenSSL error: " << buf;
  }
}

static bool load_ca_to_x509_trust(X509_STORE* store, const char *data, size_t len) {
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(data, len));
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
static int load_ca_to_ssl_ctx_from_mem(SSL_CTX* ssl_ctx, const std::string_view& cadata) {
  X509_STORE* store = nullptr;
  int count = 0;
  store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!store) {
    LOG(WARNING) << "Can't get SSL CTX cert store";
    goto out;
  }
  for (size_t pos = 0, end = pos; end < cadata.size(); pos = end) {
    end = cadata.find(kEndCertificateMark, pos);
    if (end == std::string_view::npos) {
      break;
    }
    end += sizeof(kEndCertificateMark) -1;

    std::string_view cacert(cadata.data() + pos, end - pos);
    if (load_ca_to_x509_trust(store, cacert.data(), cacert.size())) {
      ++count;
    }
  }

out:
  VLOG(2) << "Loading ca from memory: " << count << " certificates";
  return count;
}

static int load_ca_to_ssl_ctx_bundle(SSL_CTX* ssl_ctx, const std::string &bundle_path) {
  PlatformFile pf = OpenReadFile(bundle_path);
  if (pf == gurl_base::kInvalidPlatformFile) {
    return 0;
  }
  gurl_base::MemoryMappedFile mappedFile;
  // take ownship of pf
  if (!(mappedFile.Initialize(pf, gurl_base::MemoryMappedFile::Region::kWholeFile))) {
    LOG(ERROR) << "Couldn't mmap file: " << bundle_path;
    return 0;  // To debug http://crbug.com/445616.
  }

  std::string_view buffer(reinterpret_cast<const char*>(mappedFile.data()), mappedFile.length());

  return load_ca_to_ssl_ctx_from_mem(ssl_ctx, buffer);
}

static int load_ca_to_ssl_ctx_path(SSL_CTX* ssl_ctx, const std::string &dir_path) {
  int count = 0;

#ifdef _WIN32
  std::wstring wdir_path = SysUTF8ToWide(dir_path);
  _WDIR *dir;
  struct _wdirent *dent;
  dir = _wopendir(wdir_path.c_str());
  if (dir != nullptr) {
    while((dent=_wreaddir(dir))!=nullptr) {
      if (dent->d_type != DT_REG && dent->d_type != DT_LNK) {
        continue;
      }
      std::filesystem::path wca_bundle = std::filesystem::path(wdir_path) / dent->d_name;
      std::string ca_bundle = SysWideToUTF8(wca_bundle);
      int result = load_ca_to_ssl_ctx_bundle(ssl_ctx, ca_bundle);
      if (result > 0) {
        VLOG(1) << "Loading ca cert from: " << ca_bundle << " with " << result << " certificates";
        count += result;
      }
    }
    _wclosedir(dir);
  }
#else
  DIR *dir;
  struct dirent *dent;
  dir = opendir(dir_path.c_str());
  if (dir != nullptr) {
    while((dent=readdir(dir))!=nullptr) {
      if (dent->d_type != DT_REG && dent->d_type != DT_LNK) {
        continue;
      }
      std::string ca_bundle = dir_path + "/" + dent->d_name;
      int result = load_ca_to_ssl_ctx_bundle(ssl_ctx, ca_bundle);
      if (result > 0) {
        VLOG(1) << "Loading ca cert from: " << ca_bundle << " with " << result << " certificates";
        count += result;
      }
    }
    closedir(dir);
  }
#endif

  return count;
}

static int load_ca_to_ssl_ctx_cacert(SSL_CTX* ssl_ctx) {
  int count = 0;
  std::string ca_bundle = absl::GetFlag(FLAGS_cacert);
  if (!ca_bundle.empty()) {
    int result = load_ca_to_ssl_ctx_bundle(ssl_ctx, ca_bundle);
    if (result > 0) {
      LOG(INFO) << "Loading ca bundle from: " << ca_bundle << " with " << result << " certificates";
      count += result;
    } else {
      print_openssl_error();
    }
    return result;
  }
  std::string ca_path = absl::GetFlag(FLAGS_capath);
  if (!ca_path.empty()) {

    int result = load_ca_to_ssl_ctx_path(ssl_ctx, ca_path);
    if (result > 0) {
      LOG(INFO) << "Loading ca from directory: " << ca_path << " with " << result << " certificates";
      count += result;
    }
  }
  return count;
}

static int load_ca_to_ssl_ctx_yass_ca_bundle(SSL_CTX* ssl_ctx) {
#ifdef _WIN32
#define CA_BUNDLE L"yass-ca-bundle.crt"
  // The windows version will automatically look for a CA certs file named 'ca-bundle.crt',
  // either in the same directory as yass.exe, or in the Current Working Directory,
  // or in any folder along your PATH.

  std::vector<std::filesystem::path> ca_bundles;

  // 1. search under executable directory
  std::wstring exe_path;
  CHECK(GetExecutablePath(&exe_path));
  std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();

  ca_bundles.push_back(exe_dir / CA_BUNDLE);

  // 2. search under current directory
  std::wstring current_dir;
  {
    wchar_t buf[32767];
    DWORD ret = GetCurrentDirectoryW(sizeof(buf), buf);
    if (ret == 0) {
      PLOG(FATAL) << "GetCurrentDirectoryW failed";
    }
    // the return value specifies the number of characters that are written to
    // the buffer, not including the terminating null character.
    current_dir = std::wstring(buf, ret);
  }

  ca_bundles.push_back(std::filesystem::path(current_dir) / CA_BUNDLE);

  // 3. search under path directory
  std::string path;
  {
    wchar_t buf[32767];
    DWORD ret = GetEnvironmentVariableW(L"PATH", buf, sizeof(buf));
    if (ret == 0) {
      PLOG(FATAL) << "GetEnvironmentVariableW failed on PATH";
    }
    // the return value is the number of characters stored in the buffer pointed
    // to by lpBuffer, not including the terminating null character.
    path = SysWideToUTF8(std::wstring(buf, ret));
  }
  std::vector<std::string> paths = absl::StrSplit(path, ';');
  for (const auto& path : paths) {
    if (path.empty())
      continue;
    ca_bundles.push_back(std::filesystem::path(path) / CA_BUNDLE);
  }

  for (const auto &wca_bundle : ca_bundles) {
    auto ca_bundle = SysWideToUTF8(wca_bundle);
    VLOG(1) << "Trying to load ca bundle from: " << ca_bundle;
    int result = load_ca_to_ssl_ctx_bundle(ssl_ctx, ca_bundle);
    if (result > 0) {
      LOG(INFO) << "Loading ca bundle from: " << ca_bundle << " with " << result << " certificates";
      return result;
    }
  }
#undef CA_BUNDLE
#endif

  return 0;
}

static int load_ca_to_ssl_ctx_system(SSL_CTX* ssl_ctx) {
#ifdef _WIN32
  HCERTSTORE cert_store = NULL;
  asio::error_code ec;
  PCCERT_CONTEXT cert = nullptr;
  X509_STORE* store = nullptr;
  int count = 0;

  cert_store = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL,
                             CERT_SYSTEM_STORE_CURRENT_USER, L"ROOT");
  if (!cert_store) {
    PLOG(WARNING) << "CertOpenStore failed";
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
    bssl::UniquePtr<CRYPTO_BUFFER> buffer = net::x509_util::CreateCryptoBuffer(
        std::string_view(data, len));
    bssl::UniquePtr<X509> cert(X509_parse_from_buffer(buffer.get()));
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
  LOG(INFO) << "Loading ca from SChannel: " << count << " certificates";
  return count;
#elif BUILDFLAG(IS_IOS)
  return 0;
#elif BUILDFLAG(IS_MAC)
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
      bssl::UniquePtr<CRYPTO_BUFFER> buffer = net::x509_util::CreateCryptoBuffer(
          std::string_view(data, len));
      bssl::UniquePtr<X509> cert(X509_parse_from_buffer(buffer.get()));
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
  LOG(INFO) << "Loading ca from Sec: " << count << " certificates";
  return count;
#else
  int count = 0;
  // cert list copied from golang src/crypto/x509/root_unix.go
  static const char *ca_bundle_paths[] = {
    "/etc/ssl/certs/ca-certificates.crt",     // Debian/Ubuntu/Gentoo etc.
    "/etc/pki/tls/certs/ca-bundle.crt",       // Fedora/RHEL
    "/etc/ssl/ca-bundle.pem",                 // OpenSUSE
    "/etc/openssl/certs/ca-certificates.crt", // NetBSD
    "/etc/ssl/cert.pem",                      // OpenBSD
    "/usr/local/share/certs/ca-root-nss.crt", // FreeBSD/DragonFly
    "/etc/pki/tls/cacert.pem",                // OpenELEC
    "/etc/certs/ca-certificates.crt",         // Solaris 11.2+
  };
  for (auto ca_bundle : ca_bundle_paths) {
    int result = load_ca_to_ssl_ctx_bundle(ssl_ctx, ca_bundle);
    if (result > 0) {
      LOG(INFO) << "Loading ca bundle from: " << ca_bundle << " with " << result << " certificates";
      count += result;
    }
  }
  static const char *ca_paths[] = {
    "/etc/ssl/certs",               // SLES10/SLES11, https://golang.org/issue/12139
    "/etc/pki/tls/certs",           // Fedora/RHEL
    "/system/etc/security/cacerts", // Android
  };

  for (auto ca_path : ca_paths) {
    int result = load_ca_to_ssl_ctx_path(ssl_ctx, ca_path);
    if (result > 0) {
      LOG(INFO) << "Loading ca from directory: " << ca_path << " with " << result << " certificates";
      count += result;
    }
  }
  return count;
#endif
}

// loading ca certificates:
// 1. load --capath and --cacert certificates
// 2. load ca bundle from in sequence
//    - builtin ca bundle if specified
//    - yass-ca-bundle.crt if present (windows)
//    - system ca certificates
// 3. force fallback to builtin ca bundle if step 2 failes
void load_ca_to_ssl_ctx(SSL_CTX* ssl_ctx) {
  load_ca_to_ssl_ctx_cacert(ssl_ctx);

#ifdef HAVE_BUILTIN_CA_BUNDLE_CRT
  if (absl::GetFlag(FLAGS_use_ca_bundle_crt)) {
    std::string_view ca_bundle_content(_binary_ca_bundle_crt_start, _binary_ca_bundle_crt_end - _binary_ca_bundle_crt_start);
    int result = load_ca_to_ssl_ctx_from_mem(ssl_ctx, ca_bundle_content);
    LOG(WARNING) << "Builtin ca bundle loaded: " << result << " ceritificates";
    return;
  }
#endif // HAVE_BUILTIN_CA_BUNDLE_CRT

  if (load_ca_to_ssl_ctx_yass_ca_bundle(ssl_ctx) == 0 && load_ca_to_ssl_ctx_system(ssl_ctx) == 0) {
    LOG(WARNING) << "No ceritifcates from system keychain loaded, trying builtin ca bundle";

#ifdef HAVE_BUILTIN_CA_BUNDLE_CRT
    std::string_view ca_bundle_content(_binary_ca_bundle_crt_start, _binary_ca_bundle_crt_end - _binary_ca_bundle_crt_start);
    int result = load_ca_to_ssl_ctx_from_mem(ssl_ctx, ca_bundle_content);
    LOG(WARNING) << "Builtin ca bundle loaded: " << result << " ceritificates";
#else
    LOG(WARNING) << "Builtin ca bundle not available";
#endif
  }
}
