// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifdef HAVE_ICU

#include "i18n/icu_util.hpp"
#include "core/debug.hpp"
#include "core/logging.hpp"
#include "core/utils.hpp"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/utrace.h"

#if defined(ANDROID)
#include "third_party/icu/source/common/unicode/unistr.h"
#endif

#if defined(ANDROID) || defined(__linux__)
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif

#include <filesystem>

namespace {

#if DCHECK_IS_ON()
// Assert that we are not called more than once.  Even though calling this
// function isn't harmful (ICU can handle it), being called twice probably
// indicates a programming error.
bool g_check_called_once = true;
bool g_called_once = false;
#endif  // DCHECK_IS_ON()

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

// To debug http://crbug.com/445616.
int g_debug_icu_last_error;
int g_debug_icu_load;
int g_debug_icu_pf_error_details;
int g_debug_icu_pf_last_error;
#ifdef _WIN32
wchar_t g_debug_icu_pf_filename[_MAX_PATH];
#endif  // _WIN32
// Use an unversioned file name to simplify a icu version update down the road.
// No need to change the filename in multiple places (gyp files, windows
// build pkg configurations, etc). 'l' stands for Little Endian.
// This variable is exported through the header file.
const char kIcuDataFileName[] = "icudtl.dat";

// Time zone data loading.

#if defined(ANDROID)
const char kAndroidAssetsIcuDataFileName[] = "assets/icudtl.dat";
#endif  // defined(ANDROID)

// File handle intentionally never closed. Not using File here because its
// Windows implementation guards against two instances owning the same
// PlatformFile (which we allow since we know it is never freed).
PlatformFile g_icudtl_pf = kInvalidPlatformFile;
MemoryMappedFile* g_icudtl_mapped_file = nullptr;
MemoryMappedFile::Region g_icudtl_region;

void LazyInitIcuDataFile() {
  if (g_icudtl_pf != kInvalidPlatformFile) {
    return;
  }
#if 0
#if defined(ANDROID)
  int fd =
      android::OpenApkAsset(kAndroidAssetsIcuDataFileName, &g_icudtl_region);
  g_icudtl_pf = fd;
  if (fd != -1) {
    return;
  }
#endif  // defined(ANDROID)
  // For unit tests, data file is located on disk, so try there as a fallback.
#if !defined(__APPLE__)
  FilePath data_path;
  if (!PathService::Get(DIR_ASSETS, &data_path)) {
    LOG(ERROR) << "Can't find " << kIcuDataFileName;
    return;
  }
#if defined(_WIN32)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer, data_path.value().c_str());
  Alias(tmp_buffer);
#endif
  data_path = data_path.AppendASCII(kIcuDataFileName);

#if defined(_WIN32)
  // TODO(brucedawson): http://crbug.com/445616
  wchar_t tmp_buffer2[_MAX_PATH] = {0};
  wcscpy_s(tmp_buffer2, data_path.value().c_str());
  Alias(tmp_buffer2);
#endif

#else  // !defined(__APPLE__)
  // Assume it is in the framework bundle's Resources directory.
  FilePath data_path = apple::PathForFrameworkBundleResource(kIcuDataFileName);
#if 0 // BUILDFLAG(IS_IOS)
  FilePath override_data_path = ios::FilePathOfEmbeddedICU();
  if (!override_data_path.empty()) {
    data_path = override_data_path;
  }
#endif  // !BUILDFLAG(IS_IOS)
  if (data_path.empty()) {
    LOG(ERROR) << kIcuDataFileName << " not found in bundle";
    return;
  }
#endif  // !defined(__APPLE__)
#else // 0
#ifdef _WIN32
  std::wstring exe_path;
  CHECK(GetExecutablePathW(&exe_path));
  std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
  std::wstring data_path = exe_dir / SysUTF8ToWide(kIcuDataFileName);
  PlatformFile pf = OpenReadFileW(data_path);
#else // _WIN32
  std::string exe_path;
  CHECK(GetExecutablePath(&exe_path));
  std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
  std::string data_path;
  PlatformFile pf = kInvalidPlatformFile;
#ifdef __APPLE__
  data_path = exe_dir.parent_path() / "Resources" / kIcuDataFileName;
  pf = OpenReadFile(data_path);
#endif //  __APPLE__
  if (pf == kInvalidPlatformFile) {
    data_path = exe_dir / kIcuDataFileName;
    pf = OpenReadFile(data_path);
  }
#endif // _WIN32
#endif // 0
  if (pf != kInvalidPlatformFile) {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = 0;
    g_debug_icu_pf_error_details = 0;
#if defined(_WIN32)
    g_debug_icu_pf_filename[0] = 0;
#endif  // defined(_WIN32)

    g_icudtl_pf = pf;
    g_icudtl_region = MemoryMappedFile::Region::kWholeFile;
  }
#if defined(_WIN32)
  else {
    // TODO(brucedawson): http://crbug.com/445616.
    g_debug_icu_pf_last_error = ::GetLastError();
    g_debug_icu_pf_error_details = 0;
    wcscpy_s(g_debug_icu_pf_filename, data_path.c_str());
  }
#endif  // defined(_WIN32)
}

// Configures ICU to load external time zone data, if appropriate.
void InitializeExternalTimeZoneData() {}

int LoadIcuData(PlatformFile data_fd,
                const MemoryMappedFile::Region& data_region,
                std::unique_ptr<MemoryMappedFile>* out_mapped_data_file,
                UErrorCode* out_error_code) {
  InitializeExternalTimeZoneData();

  if (data_fd == kInvalidPlatformFile) {
    LOG(ERROR) << "Invalid file descriptor to ICU data received.";
    return 1;  // To debug http://crbug.com/445616.
  }

  *out_mapped_data_file = std::make_unique<MemoryMappedFile>();
  if (!(*out_mapped_data_file)->Initialize(data_fd, data_region)) {
    LOG(ERROR) << "Couldn't mmap icu data file";
    return 2;  // To debug http://crbug.com/445616.
  }

  (*out_error_code) = U_ZERO_ERROR;
  udata_setCommonData(const_cast<uint8_t*>((*out_mapped_data_file)->data()),
                      out_error_code);
  if (U_FAILURE(*out_error_code)) {
    LOG(ERROR) << "Failed to initialize ICU with data file: "
               << u_errorName(*out_error_code);
    return 3;  // To debug http://crbug.com/445616.
  }

  return 0;
}

bool InitializeICUWithFileDescriptorInternal(
    PlatformFile data_fd,
    const MemoryMappedFile::Region& data_region) {
  // This can be called multiple times in tests.
  if (g_icudtl_mapped_file) {
    g_debug_icu_load = 0;  // To debug http://crbug.com/445616.
    return true;
  }

  std::unique_ptr<MemoryMappedFile> mapped_file;
  UErrorCode err;
  g_debug_icu_load = LoadIcuData(data_fd, data_region, &mapped_file, &err);
  if (g_debug_icu_load == 1 || g_debug_icu_load == 2) {
    return false;
  }
  g_icudtl_mapped_file = mapped_file.release();

  if (g_debug_icu_load == 3) {
    g_debug_icu_last_error = err;
  }

  // Never try to load ICU data from files.
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return U_SUCCESS(err);
}

bool InitializeICUFromDataFile() {
  // If the ICU data directory is set, ICU won't actually load the data until
  // it is needed.  This can fail if the process is sandboxed at that time.
  // Instead, we map the file in and hand off the data so the sandbox won't
  // cause any problems.
  LazyInitIcuDataFile();
  bool result =
      InitializeICUWithFileDescriptorInternal(g_icudtl_pf, g_icudtl_region);

  int debug_icu_load = g_debug_icu_load;
  Alias(&debug_icu_load);
  int debug_icu_last_error = g_debug_icu_last_error;
  Alias(&debug_icu_last_error);
#if defined(_WIN32)
  int debug_icu_pf_last_error = g_debug_icu_pf_last_error;
  Alias(&debug_icu_pf_last_error);
  int debug_icu_pf_error_details = g_debug_icu_pf_error_details;
  Alias(&debug_icu_pf_error_details);
  wchar_t debug_icu_pf_filename[_MAX_PATH] = {0};
  wcscpy_s(debug_icu_pf_filename, g_debug_icu_pf_filename);
  Alias(&debug_icu_pf_filename);
#endif            // defined(_WIN32)
  CHECK(result);
  LOG(INFO) << "ICU Initialized";

  return result;
}
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

// Explicitly initialize ICU's time zone if necessary.
// On some platforms, the time zone must be explicitly initialized zone rather
// than relying on ICU's internal initialization.
void InitializeIcuTimeZone() {
#if defined(ANDROID) && 0
  // On Android, we can't leave it up to ICU to set the default time zone
  // because ICU's time zone detection does not work in many time zones (e.g.
  // Australia/Sydney, Asia/Seoul, Europe/Paris ). Use JNI to detect the host
  // time zone and set the ICU default time zone accordingly in advance of
  // actual use. See crbug.com/722821 and
  // https://ssl.icu-project.org/trac/ticket/13208 .
  std::u16string zone_id = android::GetDefaultTimeZoneId();
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(
      icu::UnicodeString(false, zone_id.data(), zone_id.length())));
#elif defined(__linux__)
  // To respond to the time zone change properly, the default time zone
  // cache in ICU has to be populated on starting up.
  // See TimeZoneMonitorLinux::NotifyClientsFromImpl().
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
#endif  // defined(ANDROID)
}

enum class ICUCreateInstance {
  kCharacterBreakIterator = 0,
  kWordBreakIterator = 1,
  kLineBreakIterator = 2,
  kLineBreakIteratorTypeLoose = 3,
  kLineBreakIteratorTypeNormal = 4,
  kLineBreakIteratorTypeStrict = 5,
  kSentenceBreakIterator = 6,
  kTitleBreakIterator = 7,
  kThaiBreakEngine = 8,
  kLaoBreakEngine = 9,
  kBurmeseBreakEngine = 10,
  kKhmerBreakEngine = 11,
  kChineseJapaneseBreakEngine = 12,

  kMaxValue = kChineseJapaneseBreakEngine
};

// Common initialization to run regardless of how ICU is initialized.
// There are multiple exposed InitializeIcu* functions. This should be called
// as at the end of (the last functions in the sequence of) these functions.
bool DoCommonInitialization() {
  // TODO(jungshik): Some callers do not care about tz at all. If necessary,
  // add a boolean argument to this function to init the default tz only
  // when requested.
  InitializeIcuTimeZone();

  utrace_setLevel(UTRACE_VERBOSE);
  return true;
}

}  // namespace

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
bool InitializeICUWithFileDescriptor(
    PlatformFile data_fd,
    const MemoryMappedFile::Region& data_region) {
#if DCHECK_IS_ON()
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif
  if (!InitializeICUWithFileDescriptorInternal(data_fd, data_region))
    return false;

  return DoCommonInitialization();
}

PlatformFile GetIcuDataFileHandle(MemoryMappedFile::Region* out_region) {
  CHECK_NE(g_icudtl_pf, kInvalidPlatformFile);
  *out_region = g_icudtl_region;
  return g_icudtl_pf;
}

void ResetGlobalsForTesting() {
  g_icudtl_pf = kInvalidPlatformFile;
  g_icudtl_mapped_file = nullptr;
#if BUILDFLAG(IS_FUCHSIA)
  g_icu_time_zone_data_dir = kIcuTimeZoneDataDir;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

bool InitializeICU() {
#if DCHECK_IS_ON()
  DCHECK(!g_check_called_once || !g_called_once);
  g_called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // The ICU data is statically linked.
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
  if (!InitializeICUFromDataFile())
    return false;
#else
#error Unsupported ICU_UTIL_DATA_IMPL value
#endif  // (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)

  return DoCommonInitialization();
}

void AllowMultipleInitializeCallsForTesting() {
#if DCHECK_IS_ON()
  g_check_called_once = false;
#endif
}

#endif // HAVE_ICU
