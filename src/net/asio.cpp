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
#include "base/apple/foundation_util.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/cert_issuer_source_static.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"
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

#if BUILDFLAG(IS_MAC)

namespace {

// The rules for interpreting trust settings are documented at:
// https://developer.apple.com/reference/security/1400261-sectrustsettingscopytrustsetting?language=objc

// Indicates the trust status of a certificate.
enum class TrustStatus {
  // Trust status is unknown / uninitialized.
  UNKNOWN,
  // Certificate inherits trust value from its issuer. If the certificate is the
  // root of the chain, this implies distrust.
  UNSPECIFIED,
  // Certificate is a trust anchor.
  TRUSTED,
  // Certificate is blocked / explicitly distrusted.
  DISTRUSTED
};

// Returns trust status of usage constraints dictionary |trust_dict| for a
// certificate that |is_self_issued|.
TrustStatus IsTrustDictionaryTrustedForPolicy(
    CFDictionaryRef trust_dict,
    bool is_self_issued,
    const CFStringRef target_policy_oid) {
  // An empty trust dict should be interpreted as
  // kSecTrustSettingsResultTrustRoot. This is handled by falling through all
  // the conditions below with the default value of |trust_settings_result|.

  // Trust settings may be scoped to a single application, by checking that the
  // code signing identity of the current application matches the serialized
  // code signing identity in the kSecTrustSettingsApplication key.
  // As this is not presently supported, skip any trust settings scoped to the
  // application.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsApplication))
    return TrustStatus::UNSPECIFIED;

  // Trust settings may be scoped using policy-specific constraints. For
  // example, SSL trust settings might be scoped to a single hostname, or EAP
  // settings specific to a particular WiFi network.
  // As this is not presently supported, skip any policy-specific trust
  // settings.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicyString))
    return TrustStatus::UNSPECIFIED;

  // Ignoring kSecTrustSettingsKeyUsage for now; it does not seem relevant to
  // the TLS case.

  // If the trust settings are scoped to a specific policy (via
  // kSecTrustSettingsPolicy), ensure that the policy is the same policy as
  // |target_policy_oid|. If there is no kSecTrustSettingsPolicy key, it's
  // considered a match for all policies.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicy)) {
    SecPolicyRef policy_ref = gurl_base::apple::GetValueFromDictionary<SecPolicyRef>(
        trust_dict, kSecTrustSettingsPolicy);
    if (!policy_ref) {
      return TrustStatus::UNSPECIFIED;
    }
    gurl_base::apple::ScopedCFTypeRef<CFDictionaryRef> policy_dict(
        SecPolicyCopyProperties(policy_ref));

    // kSecPolicyOid is guaranteed to be present in the policy dictionary.
    CFStringRef policy_oid = gurl_base::apple::GetValueFromDictionary<CFStringRef>(
        policy_dict.get(), kSecPolicyOid);

    if (!CFEqual(policy_oid, target_policy_oid))
      return TrustStatus::UNSPECIFIED;
  }

  // If kSecTrustSettingsResult is not present in the trust dict,
  // kSecTrustSettingsResultTrustRoot is assumed.
  int trust_settings_result = kSecTrustSettingsResultTrustRoot;
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsResult)) {
    CFNumberRef trust_settings_result_ref =
        gurl_base::apple::GetValueFromDictionary<CFNumberRef>(
            trust_dict, kSecTrustSettingsResult);
    if (!trust_settings_result_ref ||
        !CFNumberGetValue(trust_settings_result_ref, kCFNumberIntType,
                          &trust_settings_result)) {
      return TrustStatus::UNSPECIFIED;
    }
  }

  if (trust_settings_result == kSecTrustSettingsResultDeny)
    return TrustStatus::DISTRUSTED;

  // This is a bit of a hack: if the cert is self-issued allow either
  // kSecTrustSettingsResultTrustRoot or kSecTrustSettingsResultTrustAsRoot on
  // the basis that SecTrustSetTrustSettings should not allow creating an
  // invalid trust record in the first place. (The spec is that
  // kSecTrustSettingsResultTrustRoot can only be applied to root(self-signed)
  // certs and kSecTrustSettingsResultTrustAsRoot is used for other certs.)
  // This hack avoids having to check the signature on the cert which is slow
  // if using the platform APIs, and may require supporting MD5 signature
  // algorithms on some older OSX versions or locally added roots, which is
  // undesirable in the built-in signature verifier.
  if (is_self_issued) {
    return (trust_settings_result == kSecTrustSettingsResultTrustRoot ||
            trust_settings_result == kSecTrustSettingsResultTrustAsRoot)
               ? TrustStatus::TRUSTED
               : TrustStatus::UNSPECIFIED;
  }

  // kSecTrustSettingsResultTrustAsRoot can only be applied to non-root certs.
  return (trust_settings_result == kSecTrustSettingsResultTrustAsRoot)
             ? TrustStatus::TRUSTED
             : TrustStatus::UNSPECIFIED;
}

// Returns true if the trust settings array |trust_settings| for a certificate
// that |is_self_issued| should be treated as a trust anchor.
TrustStatus IsTrustSettingsTrustedForPolicy(CFArrayRef trust_settings,
                                            bool is_self_issued,
                                            const CFStringRef policy_oid) {
  // An empty trust settings array (that is, the trust_settings parameter
  // returns a valid but empty CFArray) means "always trust this certificate"
  // with an overall trust setting for the certificate of
  // kSecTrustSettingsResultTrustRoot.
  if (CFArrayGetCount(trust_settings) == 0) {
    return is_self_issued ? TrustStatus::TRUSTED : TrustStatus::UNSPECIFIED;
  }

  for (CFIndex i = 0, settings_count = CFArrayGetCount(trust_settings);
       i < settings_count; ++i) {
    CFDictionaryRef trust_dict = reinterpret_cast<CFDictionaryRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(trust_settings, i)));
    TrustStatus trust = IsTrustDictionaryTrustedForPolicy(
        trust_dict, is_self_issued, policy_oid);
    if (trust != TrustStatus::UNSPECIFIED)
      return trust;
  }
  return TrustStatus::UNSPECIFIED;
}

TrustStatus IsCertificateTrustedForPolicy(const bssl::ParsedCertificate* cert,
                                          SecCertificateRef cert_handle,
                                          const CFStringRef policy_oid) {
  const bool is_self_issued =
      cert->normalized_subject() == cert->normalized_issuer();

  // Evaluate user trust domain, then admin. User settings can override
  // admin (and both override the system domain, but we don't check that).
  for (const auto& trust_domain :
       {kSecTrustSettingsDomainUser, kSecTrustSettingsDomainAdmin}) {
    gurl_base::apple::ScopedCFTypeRef<CFArrayRef> trust_settings;
    OSStatus err;
    err = SecTrustSettingsCopyTrustSettings(cert_handle, trust_domain,
                                            trust_settings.InitializeInto());
    if (err != errSecSuccess) {
      if (err == errSecItemNotFound) {
        // No trust settings for that domain.. try the next.
        continue;
      }
      // OSSTATUS_LOG(ERROR, err) << "SecTrustSettingsCopyTrustSettings error";
      LOG(ERROR) << "SecTrustSettingsCopyTrustSettings error: " << DescriptionFromOSStatus(err);
      continue;
    }
    TrustStatus trust = IsTrustSettingsTrustedForPolicy(
        trust_settings.get(), is_self_issued, policy_oid);
    if (trust != TrustStatus::UNSPECIFIED)
      return trust;
  }

  // No trust settings, or none of the settings were for the correct policy, or
  // had the correct trust result.
  return TrustStatus::UNSPECIFIED;
}

// Helper method to check if an EKU is present in a std::vector of EKUs.
bool HasEKU(const std::vector<bssl::der::Input>& list, const bssl::der::Input& eku) {
  for (const auto& oid : list) {
    if (oid == eku)
      return true;
  }
  return false;
}

// Returns true if |cert| would never be a valid intermediate. (A return
// value of false does not imply that it is valid.) This is an optimization
// to avoid using memory for caching certs that would never lead to a valid
// chain. It's not intended to exhaustively test everything that
// VerifyCertificateChain does, just to filter out some of the most obviously
// unusable certs.
bool IsNotAcceptableIntermediate(const bssl::ParsedCertificate* cert,
                                 const CFStringRef policy_oid) {
  if (!cert->has_basic_constraints() || !cert->basic_constraints().is_ca) {
    return true;
  }

  // EKU filter is only implemented for TLS server auth since that's all we
  // actually care about.
  if (cert->has_extended_key_usage() &&
      CFEqual(policy_oid, kSecPolicyAppleSSL) &&
      !HasEKU(cert->extended_key_usage(), bssl::der::Input(bssl::kAnyEKU)) &&
      !HasEKU(cert->extended_key_usage(), bssl::der::Input(bssl::kServerAuth))) {
    return true;
  }

  // TODO(mattm): filter on other things too? (key usage, ...?)
  return false;
}

} // namespace
#endif

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
  const SecTrustSettingsDomain domain = kSecTrustSettingsDomainSystem;
  const CFStringRef policy_oid = kSecPolicyAppleSSL;
  CFArrayRef certs;
  OSStatus err;
  asio::error_code ec;
  CFIndex size;
  X509_STORE* store = nullptr;
  int count = 0;

  err = SecTrustSettingsCopyCertificates(domain, &certs);
  if (err != errSecSuccess) {
    LOG(ERROR) << "SecTrustSettingsCopyCertificates error: " << DescriptionFromOSStatus(err);
    goto out;
  }

  store = SSL_CTX_get_cert_store(ssl_ctx);
  if (!store) {
    LOG(WARNING) << "Can't get SSL CTX cert store";
    goto out;
  }

  size = CFArrayGetCount(certs);
  for (CFIndex i = 0; i < size; ++i) {
    SecCertificateRef sec_cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, i);
    gurl_base::apple::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(sec_cert));

    if (!der_data) {
      LOG(ERROR) << "SecCertificateCopyData error";
      continue;
    }
    const char* data = (const char *)CFDataGetBytePtr(der_data.get());
    CFIndex len = CFDataGetLength(der_data.get());

    bssl::UniquePtr<CRYPTO_BUFFER> buffer = net::x509_util::CreateCryptoBuffer(
        std::string_view(data, len));

    // keep reference to buffer
    bssl::UniquePtr<X509> cert(X509_parse_from_buffer(buffer.get()));
    if (!cert) {
      print_openssl_error();
      continue;
    }

    char buf[4096] = {};
    const char* const subject_name = X509_NAME_oneline(X509_get_subject_name(cert.get()), buf, sizeof(buf));

    bssl::CertErrors errors;
    bssl::ParseCertificateOptions options;
    options.allow_invalid_serial_numbers = true;
    std::shared_ptr<const bssl::ParsedCertificate> parsed_cert =
        bssl::ParsedCertificate::Create(std::move(buffer), options, &errors);
    if (!parsed_cert) {
      LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
      continue;
    }

    TrustStatus trust_status = IsCertificateTrustedForPolicy(
      parsed_cert.get(), sec_cert, policy_oid);

    if (trust_status == TrustStatus::DISTRUSTED) {
      LOG(WARNING) << "Ignore distrusted cert: " << subject_name;
      continue;
    }

    if (IsNotAcceptableIntermediate(parsed_cert.get(), policy_oid)) {
      LOG(WARNING) << "Ignore Unacceptable cert: " << subject_name;
      continue;
    }

    if (X509_cmp_current_time(X509_get0_notBefore(cert.get())) < 0 &&
        X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0) {
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
    } else {
      LOG(WARNING) << "Ignore inactive cert: " << subject_name;
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
