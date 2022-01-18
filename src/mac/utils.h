// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef YASS_MAC_UTILS
#define YASS_MAC_UTILS

#include <AvailabilityMacros.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#include <string>

// Checks if the current application is set as a Login Item, so it will launch
// on Login. If a non-NULL pointer to is_hidden is passed, the Login Item also
// is queried for the 'hide on launch' flag.
bool CheckLoginItemStatus(bool* is_hidden);

// Adds current application to the set of Login Items with specified "hide"
// flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
void AddToLoginItems(bool hide_on_startup);

// Adds the specified application to the set of Login Items with specified
// "hide" flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
void AddToLoginItems(const std::string& app_bundle_file_path,
                     bool hide_on_startup);

// Removes the current application from the list Of Login Items.
void RemoveFromLoginItems();

// Removes the specified application from the list Of Login Items.
void RemoveFromLoginItems(const std::string& app_bundle_file_path);

// Returns true if the current process was automatically launched as a
// 'Login Item' or via Lion's Resume. Used to suppress opening windows.
bool WasLaunchedAsLoginOrResumeItem();

// Returns true if the current process was automatically launched as a
// 'Login Item' or via Resume, and the 'Reopen windows when logging back in'
// checkbox was selected by the user.  This indicates that the previous
// session should be restored.
bool WasLaunchedAsLoginItemRestoreState();

// Returns true if the current process was automatically launched as a
// 'Login Item' with 'hide on startup' flag. Used to suppress opening windows.
bool WasLaunchedAsHiddenLoginItem();

// Remove the quarantine xattr from the given file. Returns false if there was
// an error, or true otherwise.
bool RemoveQuarantineAttribute(const std::string& file_path);

namespace internal {

// Returns the system's macOS major and minor version numbers combined into an
// integer value. For example, for macOS Sierra this returns 1012, and for macOS
// Big Sur it returns 1100. Note that the accuracy returned by this function is
// as granular as the major version number of Darwin.
int MacOSVersion();

}  // namespace internal

// Run-time OS version checks. Prefer @available in Objective-C files. If that
// is not possible, use these functions instead of
// base::SysInfo::OperatingSystemVersionNumbers. Prefer the "AtLeast" and
// "AtMost" variants to those that check for a specific version, unless you know
// for sure that you need to check for a specific version.

#define DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS10_##V() {                                              \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                   \
    return internal::MacOSVersion() == 1000 + V;                          \
  }                                                                       \
  inline bool IsAtMostOS10_##V() {                                        \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                   \
    return internal::MacOSVersion() <= 1000 + V;                          \
  }

#define DEFINE_OLD_IS_OS_FUNCS(V, DEPLOYMENT_TARGET_TEST)           \
  DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsAtLeastOS10_##V() {                                 \
    DEPLOYMENT_TARGET_TEST(>=, V, true)                             \
    return internal::MacOSVersion() >= 1000 + V;                    \
  }

#define DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS##V() {                                             \
    DEPLOYMENT_TARGET_TEST(>, V, false)                               \
    return internal::MacOSVersion() == V * 100;                       \
  }                                                                   \
  inline bool IsAtMostOS##V() {                                       \
    DEPLOYMENT_TARGET_TEST(>, V, false)                               \
    return internal::MacOSVersion() <= V * 100;                       \
  }

#define DEFINE_IS_OS_FUNCS(V, DEPLOYMENT_TARGET_TEST)           \
  DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsAtLeastOS##V() {                                \
    DEPLOYMENT_TARGET_TEST(>=, V, true)                         \
    return internal::MacOSVersion() >= V * 100;                 \
  }

#define OLD_TEST_DEPLOYMENT_TARGET(OP, V, RET)                  \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_X_VERSION_10_##V) \
    return RET;
#define TEST_DEPLOYMENT_TARGET(OP, V, RET)                     \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_VERSION_##V##_0) \
    return RET;
#define IGNORE_DEPLOYMENT_TARGET(OP, V, RET)

// Notes:
// - When bumping the minimum version of the macOS required by Chromium, remove
//   lines from below corresponding to versions of the macOS no longer
//   supported. Ensure that the minimum supported version uses the
//   DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED macro. When macOS 11.0 is the
//   minimum required version, remove all the OLD versions of the macros.
// - When bumping the minimum version of the macOS SDK required to build
//   Chromium, remove the #ifdef that switches between
//   TEST_DEPLOYMENT_TARGET and IGNORE_DEPLOYMENT_TARGET.

// Versions of macOS supported at runtime but whose SDK is not supported for
// building.
DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(11, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(12, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(13, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(14, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(15, OLD_TEST_DEPLOYMENT_TARGET)

// Versions of macOS supported at runtime and whose SDK is supported for
// building.
#ifdef MAC_OS_VERSION_11_0
DEFINE_IS_OS_FUNCS(11, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(11, IGNORE_DEPLOYMENT_TARGET)
#endif

#ifdef MAC_OS_VERSION_12_0
DEFINE_IS_OS_FUNCS(12, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(12, IGNORE_DEPLOYMENT_TARGET)
#endif

#undef DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef DEFINE_OLD_IS_OS_FUNCS
#undef DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef DEFINE_IS_OS_FUNCS
#undef OLD_TEST_DEPLOYMENT_TARGET
#undef TEST_DEPLOYMENT_TARGET
#undef IGNORE_DEPLOYMENT_TARGET

// This should be infrequently used. It only makes sense to use this to avoid
// codepaths that are very likely to break on future (unreleased, untested,
// unborn) OS releases, or to log when the OS is newer than any known version.
inline bool IsOSLaterThan12_DontCallThis() {
  return !IsAtMostOS12();
}

enum class CPUType {
  kIntel,
  kTranslatedIntel,  // Rosetta
  kArm,
};

// Returns the type of CPU this is being executed on.
CPUType GetCPUType();

// Retrieve the system's model identifier string from the IOKit registry:
// for example, "MacPro4,1", "MacBookPro6,1". Returns empty string upon
// failure.
std::string GetModelIdentifier();

// Parse a model identifier string; for example, into ("MacBookPro", 6, 1).
// If any error occurs, none of the input pointers are touched.
bool ParseModelIdentifier(const std::string& ident,
                                      std::string* type,
                                      int32_t* major,
                                      int32_t* minor);

// Returns an OS name + version string. e.g.:
//
//   "macOS Version 10.14.3 (Build 18D109)"
//
// Parts of this string change based on OS locale, so it's only useful for
// displaying to the user.
std::string GetOSDisplayName();

// Returns the serial number of the macOS device.
std::string GetPlatformSerialNumber();

#endif  // YASS_MAC_UTILS
