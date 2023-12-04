// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2023 Chilledheart  */

#include "mac/utils.h"

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <CoreServices/CoreServices.h>
#import <IOKit/IOKitLib.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#include "config/config.hpp"
#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "core/process_utils.hpp"
#include "core/utils.hpp"

#include <base/apple/scoped_cftyperef.h>
#include <base/apple/foundation_util.h>
#include <base/mac/scoped_ioobject.h>
#include <absl/strings/string_view.h>
#include <absl/strings/str_split.h>
#include <base/strings/sys_string_conversions.h>

namespace {

class LoginItemsFileList {
 public:
  LoginItemsFileList() = default;
  LoginItemsFileList(const LoginItemsFileList&) = delete;
  LoginItemsFileList& operator=(const LoginItemsFileList&) = delete;
  ~LoginItemsFileList() = default;

  WARN_UNUSED_RESULT bool Initialize() {
    DCHECK(!login_items_.get()) << __func__ << " called more than once.";
    // The LSSharedFileList suite of functions has been deprecated. Instead,
    // a LoginItems helper should be registered with SMLoginItemSetEnabled()
    // https://crbug.com/1154377.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    login_items_.reset(LSSharedFileListCreate(
        nullptr, kLSSharedFileListSessionLoginItems, nullptr));
#pragma clang diagnostic pop
    DLOG_IF(ERROR, !login_items_.get()) << "Couldn't get a Login Items list.";
    return login_items_.get();
  }

  LSSharedFileListRef GetLoginFileList() {
    DCHECK(login_items_.get()) << "Initialize() failed or not called.";
    return login_items_;
  }

  // Looks into Shared File Lists corresponding to Login Items for the item
  // representing the specified bundle.  If such an item is found, returns a
  // retained reference to it. Caller is responsible for releasing the
  // reference.
  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForApp(NSURL* url) {
    DCHECK(login_items_.get()) << "Initialize() failed or not called.";

  gurl_base::apple::ScopedCFTypeRef<CFURLRef> cfurl((CFURLRef)(CFBridgingRetain(url)));

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSArray* login_items_array(
        CFBridgingRelease(LSSharedFileListCopySnapshot(login_items_, nullptr)));
#pragma clang diagnostic pop

    for (id login_item in login_items_array) {
      LSSharedFileListItemRef item =
          (LSSharedFileListItemRef)CFBridgingRetain(login_item);
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      // kLSSharedFileListDoNotMountVolumes is used so that we don't trigger
      // mounting when it's not expected by a user. Just listing the login
      // items should not cause any side-effects.
      gurl_base::apple::ScopedCFTypeRef<CFURLRef> item_url(LSSharedFileListItemCopyResolvedURL(
          item, kLSSharedFileListDoNotMountVolumes, nullptr));
#pragma clang diagnostic pop

      if (item_url && CFEqual(item_url.get(), cfurl.get())) {
        return gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef>(item,
                                                                          gurl_base::scoped_policy::RETAIN);
      }
    }

    return gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef>();
  }

  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForMainApp() {
    NSURL* url = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
    return GetLoginItemForApp(url);
  }

 private:
  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListRef> login_items_;
};

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  gurl_base::apple::ScopedCFTypeRef<CFBooleanRef> hidden(
      reinterpret_cast<CFBooleanRef>(LSSharedFileListItemCopyProperty(
          item,
          reinterpret_cast<CFStringRef>(kLSSharedFileListLoginItemHidden))));
#pragma clang diagnostic pop

  return hidden && hidden == kCFBooleanTrue;
}

}  // namespace

bool CheckLoginItemStatus(bool* is_hidden) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return false;

  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForMainApp());
  if (!item.get())
    return false;

  if (is_hidden)
    *is_hidden = IsHiddenLoginItem(item);

  return true;
}

void AddToLoginItems(bool hide_on_startup) {
  NSBundle* bundle = [NSBundle mainBundle];
  AddToLoginItems(gurl_base::SysNSStringToUTF8([bundle bundlePath]), hide_on_startup);
}

void AddToLoginItems(const std::string& app_bundle_file_path,
                     bool hide_on_startup) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return;

  NSURL* app_bundle_url = [NSURL fileURLWithPath:@(app_bundle_file_path.c_str()) isDirectory:TRUE];

  gurl_base::apple::ScopedCFTypeRef<CFURLRef> cf_app_bundle_url((CFURLRef)CFBridgingRetain(app_bundle_url));

  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(login_items.GetLoginItemForApp(app_bundle_url));

  if (item.get() && (IsHiddenLoginItem(item) == hide_on_startup)) {
    return;  // Already is a login item with required hide flag.
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    LSSharedFileListItemRemove(login_items.GetLoginFileList(), item);
#pragma clang diagnostic pop
  }

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      @{CFBridgingRelease(CFRetain(kLSSharedFileListLoginItemHidden)) : @(hide)};
  gurl_base::apple::ScopedCFTypeRef<CFDictionaryRef> cfproperties((CFDictionaryRef)CFBridgingRetain(properties));

  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> new_item(
      LSSharedFileListInsertItemURL(login_items.GetLoginFileList(),
                                    kLSSharedFileListItemLast, nullptr, nullptr,
                                    cf_app_bundle_url, cfproperties, nullptr));
#pragma clang diagnostic pop

  if (!new_item.get()) {
    DLOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems() {
  NSBundle* bundle = [NSBundle mainBundle];
  RemoveFromLoginItems(gurl_base::SysNSStringToUTF8([bundle bundlePath]));
}

void RemoveFromLoginItems(const std::string& app_bundle_file_path) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return;

  NSURL* app_bundle_url =
      [NSURL fileURLWithPath:@(app_bundle_file_path.c_str())];
  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForApp(app_bundle_url));
  if (!item.get())
    return;

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  LSSharedFileListItemRemove(login_items.GetLoginFileList(), item);
#pragma clang diagnostic pop
}

bool WasLaunchedAsLoginOrResumeItem() {
  ProcessSerialNumber psn = {0, kCurrentProcess};
  ProcessInfoRec info = {};
  info.processInfoLength = sizeof(info);

// GetProcessInformation has been deprecated since macOS 10.9, but there is no
// replacement that provides the information we need. See
// https://crbug.com/650854.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (GetProcessInformation(&psn, &info) == noErr) {
#pragma clang diagnostic pop
    ProcessInfoRec parent_info = {};
    parent_info.processInfoLength = sizeof(parent_info);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (GetProcessInformation(&info.processLauncher, &parent_info) == noErr) {
#pragma clang diagnostic pop
      return parent_info.processSignature == 'lgnw';
    }
  }
  return false;
}

bool WasLaunchedAsLoginItemRestoreState() {
  // "Reopen windows..." option was added for Lion.  Prior OS versions should
  // not have this behavior.
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  CFStringRef app = CFSTR("com.apple.loginwindow");
  CFStringRef save_state = CFSTR("TALLogoutSavesState");
  gurl_base::apple::ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPreferencesCopyAppValue(save_state, app));
  // According to documentation, com.apple.loginwindow.plist does not exist on a
  // fresh installation until the user changes a login window setting.  The
  // "reopen windows" option is checked by default, so the plist would exist had
  // the user unchecked it.
  // https://developer.apple.com/library/mac/documentation/macosx/conceptual/bpsystemstartup/chapters/CustomLogin.html
  if (!plist)
    return true;

  if (CFBooleanRef restore_state = CFCast<CFBooleanRef>(plist))
    return CFBooleanGetValue(restore_state);

  return false;
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  LoginItemsFileList login_items;
  if (!login_items.Initialize())
    return false;

  gurl_base::apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForMainApp());
  if (!item.get()) {
    // OS X can launch items for the resume feature.
    return false;
  }
  return IsHiddenLoginItem(item);
}

bool RemoveQuarantineAttribute(const std::string& file_path) {
  const char kQuarantineAttrName[] = "com.apple.quarantine";
  int status = removexattr(file_path.c_str(), kQuarantineAttrName, 0);
  return status == 0 || errno == ENOATTR;
}

namespace {

// Returns the running system's Darwin major version. Don't call this, it's an
// implementation detail and its result is meant to be cached by
// MacOSVersionInternal().
int DarwinMajorVersionInternal() {
  // :OperatingSystemVersionNumbers() at one time called Gestalt(), which
  // was observed to be able to spawn threads (see https://crbug.com/53200).
  // Nowadays that function calls -[NSProcessInfo operatingSystemVersion], whose
  // current implementation does things like hit the file system, which is
  // possibly a blocking operation. Either way, it's overkill for what needs to
  // be done here.
  //
  // uname, on the other hand, is implemented as a simple series of sysctl
  // system calls to obtain the relevant data from the kernel. The data is
  // compiled right into the kernel, so no threads or blocking or other
  // funny business is necessary.

  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    DPLOG(ERROR) << "uname";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0) {
    DLOG(ERROR) << "unexpected uname sysname " << uname_info.sysname;
    return 0;
  }

  int darwin_major_version = 0;
  char* dot = strchr(uname_info.release, '.');
  if (dot) {
    auto ver = StringToInteger(std::string(uname_info.release, dot - uname_info.release));

    if (!ver.ok()) {
      dot = nullptr;
    } else {
      darwin_major_version = ver.value();
    }
  }

  if (!dot) {
    DLOG(ERROR) << "could not parse uname release " << uname_info.release;
    return 0;
  }

  return darwin_major_version;
}

// The implementation of MacOSVersion() as defined in the header. Don't call
// this, it's an implementation detail and the result is meant to be cached by
// MacOSVersion().
int MacOSVersionInternal() {
  int darwin_major_version = DarwinMajorVersionInternal();

  // Darwin major versions 6 through 19 corresponded to macOS versions 10.2
  // through 10.15.
  CHECK(darwin_major_version >= 6);
  if (darwin_major_version <= 19)
    return 1000 + darwin_major_version - 4;

  // Darwin major version 20 corresponds to macOS version 11.0. Assume a
  // correspondence between Darwin's major version numbers and macOS major
  // version numbers.
  int macos_major_version = darwin_major_version - 9;

  return macos_major_version * 100;
}

}  // namespace

namespace internal {

int MacOSVersion() {
  static int macos_version = MacOSVersionInternal();
  return macos_version;
}

}  // namespace internal

namespace {

#if defined(ARCH_CPU_X86_64)
// https://developer.apple.com/documentation/apple_silicon/about_the_rosetta_translation_environment#3616845
bool ProcessIsTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
    return false;
  return ret;
}
#endif  // ARCH_CPU_X86_64

}  // namespace

CPUType GetCPUType() {
#if defined(ARCH_CPU_ARM64)
  return CPUType::kArm;
#elif defined(ARCH_CPU_X86_64)
  return ProcessIsTranslated() ? CPUType::kTranslatedIntel : CPUType::kIntel;
#else
#error Time for another chip transition?
#endif  // ARCH_CPU_*
}

std::string GetModelIdentifier() {
  std::string return_string;
  gurl_base::mac::ScopedIOObject<io_service_t> platform_expert(IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice")));
  if (platform_expert) {
    gurl_base::apple::ScopedCFTypeRef<CFDataRef> model_data(
        static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
            platform_expert, CFSTR("model"), kCFAllocatorDefault, 0)));
    if (model_data) {
      return_string =
          reinterpret_cast<const char*>(CFDataGetBytePtr(model_data));
    }
  }
  return return_string;
}

bool ParseModelIdentifier(const std::string& ident,
                          std::string* type,
                          int32_t* major,
                          int32_t* minor) {
  size_t number_loc = ident.find_first_of("0123456789");
  if (number_loc == std::string::npos)
    return false;
  size_t comma_loc = ident.find(',', number_loc);
  if (comma_loc == std::string::npos)
    return false;
  auto major_tmp = StringToInteger(
      std::string(ident.c_str() + number_loc, comma_loc - number_loc));
  auto minor_tmp =
      StringToInteger(std::string(ident.c_str() + comma_loc + 1));
  if (!major_tmp.ok() || !minor_tmp.ok())
    return false;
  *type = ident.substr(0, number_loc);
  *major = major_tmp.value();
  *minor = minor_tmp.value();
  return true;
}

std::string GetOSDisplayName() {
  std::string os_name;
  if (IsAtMostOS10_11())
    os_name = "OS X";
  else
    os_name = "macOS";
  std::string version_string = gurl_base::SysNSStringToUTF8(
      [[NSProcessInfo processInfo] operatingSystemVersionString]);
  return os_name + " " + version_string;
}

std::string GetPlatformSerialNumber() {
  gurl_base::mac::ScopedIOObject<io_service_t> expert_device(IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice")));
  if (!expert_device) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  gurl_base::apple::ScopedCFTypeRef<CFTypeRef> serial_number(IORegistryEntryCreateCFProperty(
      expert_device, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault,
      0));
  CFStringRef serial_number_cfstring = CFCast<CFStringRef>(serial_number);
  if (!serial_number_cfstring) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  return gurl_base::SysCFStringRefToUTF8(serial_number_cfstring);
}

static std::string GetLocalAddr() {
  auto local_host = absl::GetFlag(FLAGS_local_host);

  asio::error_code ec;
  auto addr = asio::ip::make_address(local_host.c_str(), ec);
  bool host_is_ip_address = !ec;
  if (host_is_ip_address && addr.is_unspecified()) {
    if (addr.is_v6()) {
      local_host = "::1";
    } else {
      local_host = "127.0.0.1";
    }
  }
  return local_host;
}

bool GetSystemProxy() {
  bool enabled;
  std::string server_addr, bypass_addr;
  int32_t server_port;

  if (!QuerySystemProxy(&enabled, &server_addr, &server_port, &bypass_addr)) {
    return false;
  }
  auto local_host = GetLocalAddr();
  auto local_port = absl::GetFlag(FLAGS_local_port);
  return enabled && local_host == server_addr && local_port == server_port;
}

bool SetSystemProxy(bool on) {
  bool enabled;
  std::string server_addr, bypass_addr;
  int32_t server_port;

  ::QuerySystemProxy(&enabled, &server_addr, &server_port, &bypass_addr);
  if (on) {
    server_addr = GetLocalAddr();
    server_port = absl::GetFlag(FLAGS_local_port);
  }
  return ::SetSystemProxy(on, server_addr, server_port, bypass_addr);
}

bool QuerySystemProxy(bool *enabled,
                      std::string *server_addr,
                      int32_t *server_port,
                      std::string *bypass_addr) {
  *enabled = false;
  server_addr->clear();
  *server_port = 0;
  bypass_addr->clear();

  gurl_base::apple::ScopedCFTypeRef<CFDictionaryRef> proxies {SCDynamicStoreCopyProxies(nullptr)};

  {
    CFNumberRef obj;
    if (CFDictionaryGetValueIfPresent(proxies, kSCPropNetProxiesHTTPEnable,
            (const void**)&obj) &&
        CFGetTypeID(obj) == CFNumberGetTypeID() &&
        CFNumberGetValue(obj, kCFNumberSInt8Type, enabled)) {
      LOG(INFO) << "QuerySystemProxy: enabled " << *enabled;
    }
  }

  {
    CFStringRef obj;
    if (CFDictionaryGetValueIfPresent(proxies, kSCPropNetProxiesHTTPProxy,
            (const void**)&obj) &&
        CFGetTypeID(obj) == CFStringGetTypeID()) {
      *server_addr = gurl_base::SysCFStringRefToUTF8(obj);
      LOG(INFO) << "QuerySystemProxy: server_addr " << *server_addr;
    }
  }

  {
    CFNumberRef obj;
    if (CFDictionaryGetValueIfPresent(proxies, kSCPropNetProxiesHTTPPort,
            (const void**)&obj) &&
        CFGetTypeID(obj) == CFNumberGetTypeID() &&
        CFNumberGetValue(obj, kCFNumberSInt32Type, server_port)) {
      LOG(INFO) << "QuerySystemProxy: server_port " << *server_port;
    }
  }

  {
    CFArrayRef obj;
    if (CFDictionaryGetValueIfPresent(proxies, kSCPropNetProxiesExceptionsList,
            (const void**)&obj) &&
        CFGetTypeID(obj) == CFArrayGetTypeID()) {
      CFIndex array_count = CFArrayGetCount(obj);
      for (CFIndex i = 0; i < array_count; ++i) {
        CFStringRef str_obj = (CFStringRef)CFArrayGetValueAtIndex(obj, i);
        *bypass_addr = *bypass_addr + gurl_base::SysCFStringRefToUTF8(str_obj) + ", ";
      }
      if (array_count) {
        bypass_addr->resize(bypass_addr->size() - 2);
      }
      LOG(INFO) << "QuerySystemProxy: bypass " << *bypass_addr;
    }
  }
  return true;
}

bool SetSystemProxy(bool enable,
                    const std::string &server_addr,
                    int32_t server_port,
                    const std::string &bypass_addr) {
  std::string output, _;
  std::vector<std::string> params = {"/usr/sbin/networksetup", "-listallnetworkservices"};
  if (ExecuteProcess(params, &output, &_) != 0) {
    return false;
  }
  std::vector<std::string> services = absl::StrSplit(output, '\n');

  for (const auto& service: services) {
    if (service.empty()) {
      continue;
    }
    if (service.find('*') != std::string::npos) {
      continue;
    }
    LOG(INFO) << "Found service: " << service;
    if (!enable) {
      params = {"/usr/sbin/networksetup", "-setwebproxystate",
        service, "off"};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    } else {
      params = {"/usr/sbin/networksetup", "-setwebproxy",
        service, server_addr, std::to_string(server_port)};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    }
    if (!enable) {
      params = {"/usr/sbin/networksetup", "-setsecurewebproxystate",
        service, "off"};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    } else {
      params = {"/usr/sbin/networksetup", "-setsecurewebproxy",
        service, server_addr, std::to_string(server_port)};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    }
    if (!enable) {
      params = {"/usr/sbin/networksetup", "-setsocksfirewallproxystate",
        service, "off"};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    } else {
      params = {"/usr/sbin/networksetup", "-setsocksfirewallproxy",
        service, server_addr, std::to_string(server_port)};
      if (ExecuteProcess(params, &_, &_) != 0) {
        return false;
      }
    }
  }

  return true;
}
