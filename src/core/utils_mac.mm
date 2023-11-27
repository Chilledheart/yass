// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#ifdef __APPLE__

#include <absl/flags/internal/program_name.h>

#if defined(OS_APPLE) && defined(__clang__)

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>

#endif  // defined(OS_APPLE) && defined(__clang__)

#include <errno.h>
#include <locale.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>  // For mlock.
#include <sys/resource.h>

#include "core/logging.hpp"
#include "core/foundation_util.hpp"

#ifndef VM_MEMORY_MALLOC_PROB_GUARD
#define VM_MEMORY_MALLOC_PROB_GUARD 13
#endif // VM_MEMORY_MALLOC_PROB_GUARD

#ifndef VM_MEMORY_COLORSYNC
#define VM_MEMORY_COLORSYNC 104
#endif // VM_MEMORY_COLORSYNC
#ifndef VM_MEMORY_BTINFO
#define VM_MEMORY_BTINFO 105
#endif  // VM_MEMORY_BTINFO

bool SetThreadPriority(std::thread::native_handle_type handle,
                       ThreadPriority priority) {
  // Changing the priority of the main thread causes performance regressions.
  // https://crbug.com/601270
  DCHECK(!pthread_main_np());
  qos_class_t qos_class = QOS_CLASS_UNSPECIFIED;
  int relative_priority = 0; // Undocumented

  switch (priority) {
    case ThreadPriority::BACKGROUND:
      qos_class = QOS_CLASS_BACKGROUND;
      relative_priority = 0;
      break;
    case ThreadPriority::NORMAL:
      qos_class = QOS_CLASS_UTILITY;
      relative_priority = 0;
      break;
    case ThreadPriority::ABOVE_NORMAL: {
      qos_class = QOS_CLASS_USER_INITIATED;
      relative_priority = 0;
      break;
    }
    case ThreadPriority::TIME_CRITICAL:
      qos_class = QOS_CLASS_USER_INTERACTIVE;
      relative_priority = 0;
      break;
  }
  /// documented in
  /// https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
  /// and https://developer.apple.com/library/archive/documentation/Performance/Conceptual/EnergyGuide-iOS/PrioritizeWorkWithQoS.html
  return pthread_set_qos_class_self_np(qos_class, relative_priority) == 0;

}

bool SetThreadName(std::thread::native_handle_type handle,
                   const std::string& name) {
  // pthread_setname() fails (harmlessly) in the sandbox, ignore when it does.
  // See http://crbug.com/47058
  return pthread_setname_np(name.c_str()) == 0;
}

static uint64_t MachTimeToNanoseconds(uint64_t machTime) {
  uint64_t nanoseconds = 0;
  static mach_timebase_info_data_t sTimebase;
  if (sTimebase.denom == 0) {
    kern_return_t mtiStatus = mach_timebase_info(&sTimebase);
    assert(mtiStatus == KERN_SUCCESS && "mach_timebase_info failure");
    (void)mtiStatus;
  }

  nanoseconds = ((machTime * sTimebase.numer) / sTimebase.denom);

  return nanoseconds;
}

static std::string protection_str(vm_prot_t prot) {
  std::string returned(4, '\0');

  returned[0] = (prot & VM_PROT_READ ? 'r' : '-');
  returned[1] = (prot & VM_PROT_WRITE ? 'w' : '-');
  returned[2] = (prot & VM_PROT_EXECUTE ? 'x' : '-');
  returned[3] = '\0';

  return returned;
}

static const char* behavior_str(vm_behavior_t behavior) {
  switch (behavior) {
    case VM_BEHAVIOR_DEFAULT:
      return "default";
    case VM_BEHAVIOR_RANDOM:
      return "random";
    case VM_BEHAVIOR_SEQUENTIAL:
      return "fwd-seq";
    case VM_BEHAVIOR_RSEQNTL:
      return "rev-seq";
    case VM_BEHAVIOR_WILLNEED:
      return "will-need";
    case VM_BEHAVIOR_DONTNEED:
      return "will-need";
    case VM_BEHAVIOR_FREE:
      return "free-nowb";
    case VM_BEHAVIOR_ZERO_WIRED_PAGES:
      return "zero-wire";
    case VM_BEHAVIOR_REUSABLE:
      return "reusable";
    case VM_BEHAVIOR_REUSE:
      return "reuse";
    case VM_BEHAVIOR_CAN_REUSE:
      return "canreuse";
    case VM_BEHAVIOR_PAGEOUT:
      return "pageout";
    default:
      return "?";
  }
}

static const char* inherit_str(vm_inherit_t inherit) {
  switch (inherit) {
    case VM_INHERIT_SHARE:
      return "share-with-child";
    case VM_INHERIT_COPY:
      return "copy-into-child";
    case VM_INHERIT_NONE:
      return "absent-from-child";
    case VM_INHERIT_DONATE_COPY:
      return "copy-and-delete";
    default:
      return "?";
  }
}

static const char* user_tag_str(unsigned int user_tag) {
  switch (user_tag) {
    case 0:
      return "none";
    case VM_MEMORY_MALLOC:
      return "malloc";
    case VM_MEMORY_MALLOC_SMALL:
      return "malloc-small";
    case VM_MEMORY_MALLOC_LARGE:
      return "malloc-large";
    case VM_MEMORY_MALLOC_HUGE:
      return "malloc-huge";
    case VM_MEMORY_SBRK:
      return "sbrk";
    case VM_MEMORY_REALLOC:
      return "realloc";
    case VM_MEMORY_MALLOC_TINY:
      return "malloc-tiny";
    case VM_MEMORY_MALLOC_LARGE_REUSABLE:
      return "malloc-large-reusable";
    case VM_MEMORY_MALLOC_LARGE_REUSED:
      return "malloc-large-reused";
    case VM_MEMORY_ANALYSIS_TOOL:
      return "malloc-analysis-tool";
    case VM_MEMORY_MALLOC_NANO:
      return "malloc-nano";
    case VM_MEMORY_MALLOC_MEDIUM:
      return "malloc-medium";
    case VM_MEMORY_MALLOC_PROB_GUARD:
      return "malloc-pguard";
    case VM_MEMORY_MACH_MSG:
      return "mach-msg";
    case VM_MEMORY_IOKIT:
      return "iokit";
    case VM_MEMORY_STACK:
      return "stack";
    case VM_MEMORY_GUARD:
      return "guard";
    case VM_MEMORY_SHARED_PMAP:
      return "shared-pmap";
    case VM_MEMORY_DYLIB:
      return "dylib";
    case VM_MEMORY_OBJC_DISPATCHERS:
      return "objc-dispatchers";
    case VM_MEMORY_UNSHARED_PMAP:
      return "unshared-pmap";
    case VM_MEMORY_APPKIT:
      return "appkit";
    case VM_MEMORY_FOUNDATION:
      return "foundation";
    case VM_MEMORY_COREGRAPHICS:
    // case VM_MEMORY_COREGRAPHICS_MISC:
      return "coregraphics";
    case VM_MEMORY_CORESERVICES:
    // case VM_MEMORY_CARBON:
      return "coreservices";
    case VM_MEMORY_JAVA:
      return "java";
    case VM_MEMORY_COREDATA:
      return "coredata";
    case VM_MEMORY_COREDATA_OBJECTIDS:
      return "coredata-objectids";
    case VM_MEMORY_ATS:
      return "ats";
    case VM_MEMORY_LAYERKIT:
      return "layerkit";
    case VM_MEMORY_CGIMAGE:
      return "cgimage";
    case VM_MEMORY_TCMALLOC:
      return "tcmalloc";
    case VM_MEMORY_COREGRAPHICS_DATA:
      return "coregraphics-data";
    case VM_MEMORY_COREGRAPHICS_SHARED:
      return "coregraphics-shared";
    case VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS:
      return "coregraphics-framebuffers";
    case VM_MEMORY_COREGRAPHICS_BACKINGSTORES:
      return "coregraphics-backingstores";
    case VM_MEMORY_COREGRAPHICS_XALLOC:
      return "coregraphics-xalloc";
    case VM_MEMORY_DYLD:
      return "dyld";
    case VM_MEMORY_DYLD_MALLOC:
      return "dyld-malloc";
    case VM_MEMORY_SQLITE:
      return "sqlite";
    case VM_MEMORY_JAVASCRIPT_CORE:
    // case VM_MEMORY_WEBASSEMBLY:
      return "javascript-core";
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR:
      return "javascript-jit-executable-allocator";
    case VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE:
      return "javascript-jit-register-file";
    case VM_MEMORY_GLSL:
      return "glsl";
    case VM_MEMORY_OPENCL:
      return "opencl";
    case VM_MEMORY_COREIMAGE:
      return "coreimage";
    case VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS:
      return "webcore-purgeable-buffers";
    case VM_MEMORY_IMAGEIO:
      return "imageio";
    case VM_MEMORY_COREPROFILE:
      return "coreprofile";
    case VM_MEMORY_ASSETSD:
      return "assetsd";
    case VM_MEMORY_OS_ALLOC_ONCE:
      return "os-alloc-once";
    case VM_MEMORY_LIBDISPATCH:
      return "libdispatch";
    case VM_MEMORY_ACCELERATE:
      return "allcelerate";
    case VM_MEMORY_COREUI:
      return "coreui";
    case VM_MEMORY_COREUIFILE:
      return "coreui-file";
    case VM_MEMORY_GENEALOGY:
      return "genealogy";
    case VM_MEMORY_RAWCAMERA:
      return "raw-camera";
    case VM_MEMORY_CORPSEINFO:
      return "corpseinfo";
    case VM_MEMORY_ASL:
      return "asl";
    case VM_MEMORY_SWIFT_RUNTIME:
      return "swift-runtime";
    case VM_MEMORY_SWIFT_METADATA:
      return "swift-metadata";
    case VM_MEMORY_DHMM:
      return "dhmm";
    case VM_MEMORY_SCENEKIT:
      return "scenekit";
    case VM_MEMORY_SKYWALK:
      return "skywalk";
    case VM_MEMORY_IOSURFACE:
      return "iosurface";
    case VM_MEMORY_LIBNETWORK:
      return "libnetwork";
    case VM_MEMORY_AUDIO:
      return "audio";
    case VM_MEMORY_VIDEOBITSTREAM:
      return "videobitstream";
    case VM_MEMORY_CM_XPC:
      return "cm-xpc";
    case VM_MEMORY_CM_RPC:
      return "cm-rpc";
    case VM_MEMORY_CM_MEMORYPOOL:
      return "cm-memorypool";
    case VM_MEMORY_CM_READCACHE:
      return "cm-readcache";
    case VM_MEMORY_CM_CRABS:
      return "cm-crabs";
    case VM_MEMORY_QUICKLOOK_THUMBNAILS:
      return "quicklook-thumbnails";
    case VM_MEMORY_ACCOUNTS:
      return "accounts";
    case VM_MEMORY_SANITIZER:
      return "sanitizers";
    case VM_MEMORY_IOACCELERATOR:
      return "ioaccelerator";
    case VM_MEMORY_CM_REGWARP:
      return "cm-regwrap";
    case VM_MEMORY_EAR_DECODER:
      return "ear-decoder";
    case VM_MEMORY_COREUI_CACHED_IMAGE_DATA:
      return "coreui-cached-image-data";
    case VM_MEMORY_COLORSYNC:
      return "colorsync";
    case VM_MEMORY_BTINFO:
      return "backtrace-info";
    case VM_MEMORY_ROSETTA:
      return "rosetta";
    case VM_MEMORY_ROSETTA_THREAD_CONTEXT:
      return "rosetta-thread-context";
    case VM_MEMORY_ROSETTA_INDIRECT_BRANCH_MAP:
      return "rosetta-indirect-branch-map";
    case VM_MEMORY_ROSETTA_RETURN_STACK:
      return "rosetta-return-stack";
    case VM_MEMORY_ROSETTA_EXECUTABLE_HEAP:
      return "rosetta-executable-heap";
    case VM_MEMORY_ROSETTA_USER_LDT:
      return "rosetta-user-ldt";
    case VM_MEMORY_ROSETTA_ARENA:
      return "rosetta-arena";
    case VM_MEMORY_ROSETTA_10:
      return "rosetta-10";
    case VM_MEMORY_APPLICATION_SPECIFIC_1:
      return "application-specific-1";
    case VM_MEMORY_APPLICATION_SPECIFIC_16:
      return "application-specific-16";
    default:
      return "?";
  }
}

static const char* share_mode_str(unsigned char share_mode) {
  switch (share_mode) {
    case SM_COW:
      return "cow";
    case SM_PRIVATE:
      return "private";
    case SM_EMPTY:
      return "empty";
    case SM_SHARED:
      return "shared";
    case SM_TRUESHARED:
      return "true-shared";
    case SM_PRIVATE_ALIASED:
      return "private-aliased";
    case SM_SHARED_ALIASED:
      return "shared-aliased";
    case SM_LARGE_PAGE:
      return "large-page";
    default:
      return "?";
  }
}

bool MemoryLockAll() {
  mach_port_t task = mach_task_self();
  bool failed = false;
  vm_address_t address = MACH_VM_MIN_ADDRESS;

  while (address < MACH_VM_MAX_ADDRESS) {
    vm_size_t vm_region_size;
    mach_msg_type_number_t count = VM_REGION_EXTENDED_INFO_COUNT;
    vm_region_extended_info_data_t vm_region_info;
    mach_port_t object_name;
    kern_return_t kr;
    kr = vm_region_64(task, &address, &vm_region_size, VM_REGION_EXTENDED_INFO,
                      (vm_region_info_t)&vm_region_info, &count, &object_name);
    if (kr == KERN_INVALID_ADDRESS) {
      // We're at the end of the address space.
      break;
    }

    if (kr != KERN_SUCCESS) {
      LOG(WARNING) << "Failed to call vm_region_64: " << kr;
      return false;
    }

    bool lockable = false;

    switch (vm_region_info.user_tag) {
      case VM_MEMORY_MALLOC:
      case VM_MEMORY_STACK:
        if (vm_region_info.protection != VM_PROT_NONE)
          lockable = true;
        break;
      case VM_MEMORY_MALLOC_SMALL:
      case VM_MEMORY_MALLOC_LARGE:
      case VM_MEMORY_MALLOC_HUGE:
      case VM_MEMORY_SBRK:
      case VM_MEMORY_REALLOC:
      case VM_MEMORY_MALLOC_TINY:
      case VM_MEMORY_MALLOC_LARGE_REUSABLE:
      case VM_MEMORY_MALLOC_LARGE_REUSED:
      case VM_MEMORY_ANALYSIS_TOOL:
      case VM_MEMORY_MALLOC_NANO:
      case VM_MEMORY_MALLOC_MEDIUM:
        lockable = true;
        break;
      case VM_MEMORY_MACH_MSG:
      case VM_MEMORY_IOKIT:
      case VM_MEMORY_DYLIB:
      case VM_MEMORY_OBJC_DISPATCHERS:
      case VM_MEMORY_MALLOC_PROB_GUARD:
      case VM_MEMORY_GUARD:
      case VM_MEMORY_SHARED_PMAP:
      case VM_MEMORY_UNSHARED_PMAP:
      default:
        lockable = false;
        break;
    }

    if (VLOG_IS_ON(4)) {
      mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
      vm_region_basic_info_data_64_t vm_basic_info {};
      mach_port_t object_name;
      vm_region_64(task, &address, &vm_region_size, VM_REGION_BASIC_INFO_64,
                   (vm_region_info_t)&vm_basic_info, &count, &object_name);
      VLOG(4) << "Calling mlock on address: " << (void*)address
              << " protection: " << protection_str(vm_region_info.protection)
              << " max_protection: "
              << protection_str(vm_basic_info.max_protection)
              << " inheritance: " << inherit_str(vm_basic_info.inheritance)
              << " shared: " << std::boolalpha << vm_basic_info.shared
              << std::dec << " reserved: " << std::boolalpha
              << vm_basic_info.reserved << std::dec << " offset: 0x" << std::hex
              << vm_basic_info.offset << std::dec
              << " behavior: " << behavior_str(vm_basic_info.behavior)
              << " user_wired_count: 0x" << vm_basic_info.user_wired_count
              << " user_tag: " << user_tag_str(vm_region_info.user_tag)
              << " pages_resident: " << vm_region_info.pages_resident
              << " pages_shared_now_private: "
              << vm_region_info.pages_shared_now_private
              << " pages_swapped_out: " << vm_region_info.pages_swapped_out
              << " pages_dirtied: " << vm_region_info.pages_dirtied
              << " ref_count: " << vm_region_info.ref_count
              << " shadow_depth: " << vm_region_info.shadow_depth
              << " external_pager: " << std::boolalpha
              << (bool)vm_region_info.external_pager << std::dec
              << " share_mode: " << share_mode_str(vm_region_info.share_mode)
              << " pages_reusable: " << vm_region_info.pages_reusable;

      // The kernel always returns a null object for VM_REGION_BASIC_INFO_64,
      // but balance it with a deallocate in case this ever changes. See 10.9.2
      // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
      mach_port_deallocate(task, object_name);
    }

    if (lockable && mlock((void*)address, vm_region_size)) {
      PLOG(WARNING) << "Failed to call mlock on address: " << (void*)address
                    << " protection: "
                    << protection_str(vm_region_info.protection)
                    << " user_tag: " << user_tag_str(vm_region_info.user_tag)
                    << " share_mode: "
                    << share_mode_str(vm_region_info.share_mode);
      failed = true;
    }

    // The kernel always returns a null object for VM_REGION_EXTENDED_INFO, but
    // balance it with a deallocate in case this ever changes. See 10.9.2
    // xnu-2422.90.20/osfmk/vm/vm_map.c vm_map_region.
    mach_port_deallocate(task, object_name);

    // Move to the next region
    address += vm_region_size;
  }

  return !failed;
}

uint64_t GetMonotonicTime() {
  uint64_t now = mach_absolute_time();
  return MachTimeToNanoseconds(now);
}

// TBD
bool IsProgramConsole() {
  return true;
}

bool SetUTF8Locale() {
  // C.UTF-8 doesn't exists on macOS
  if (setlocale(LC_ALL, "en_US.UTF-8") == nullptr)
    return false;
  if (strcmp(setlocale(LC_ALL, nullptr), "en_US.UTF-8") != 0)
    return false;
  return true;
}

#if defined(OS_APPLE) && defined(__clang__)

namespace {

// Convert the supplied CFString into the specified encoding, and return it as
// an STL string of the template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0)
    return StringType();

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring,
                                       whole_string,
                                       encoding,
                                       0,      // lossByte
                                       false,  // isExternalRepresentation
                                       nullptr,// buffer
                                       0,      // maxBufLen
                                       &out_size);
  if (converted == 0 || out_size == 0)
    return StringType();

  // out_size is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert out_size by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted = CFStringGetBytes(cfstring,
                               whole_string,
                               encoding,
                               0,      // lossByte
                               false,  // isExternalRepresentation
                               reinterpret_cast<UInt8*>(&out_buffer[0]),
                               out_size,
                               nullptr);  // usedBufLen
  if (converted == 0)
    return StringType();

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

// Given an STL string |in| with an encoding specified by |in_encoding|,
// convert it to |out_encoding| and return it as an STL string of the
// |OutStringType| template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename InStringType, typename OutStringType>
static OutStringType STLStringToSTLStringWithEncodingsT(
    const InStringType& in,
    CFStringEncoding in_encoding,
    CFStringEncoding out_encoding) {
  typename InStringType::size_type in_length = in.length();
  if (in_length == 0)
    return OutStringType();

  ScopedCFTypeRef<CFStringRef> cfstring(CFStringCreateWithBytesNoCopy(
      nullptr, reinterpret_cast<const UInt8*>(in.data()),
      in_length * sizeof(typename InStringType::value_type), in_encoding, false,
      kCFAllocatorNull));
  if (!cfstring)
    return OutStringType();

  return CFStringToSTLStringWithEncodingT<OutStringType>(cfstring,
                                                         out_encoding);
}

// Given a std::string_view|in| with an encoding specified by |in_encoding|, return
// it as a CFStringRef.  Returns NULL on failure.
template <typename CharT>
static ScopedCFTypeRef<CFStringRef> StringToCFStringWithEncodingsT(
    const std::basic_string<CharT> &in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  ScopedCFTypeRef<CFStringRef> ret;

  if (in_length == 0)
    return ScopedCFTypeRef<CFStringRef>(CFSTR(""), scoped_policy::RETAIN);

  return ScopedCFTypeRef<CFStringRef>(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
      in_length * sizeof(CharT), in_encoding, false));
}

// Given a std::string_view|in| with an encoding specified by |in_encoding|, return
// it as a CFStringRef.  Returns NULL on failure.
static ScopedCFTypeRef<CFStringRef> StringViewToCFStringWithEncodingsT(
    std::string_view in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  ScopedCFTypeRef<CFStringRef> ret;

  if (in_length == 0)
    return ScopedCFTypeRef<CFStringRef>(CFSTR(""), scoped_policy::RETAIN);

  return ScopedCFTypeRef<CFStringRef>(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
      in_length * sizeof(char), in_encoding, false));
}

// Specify the byte ordering explicitly, otherwise CFString will be confused
// when strings don't carry BOMs, as they typically won't.
static const CFStringEncoding kNarrowStringEncoding = kCFStringEncodingUTF8;
#ifdef __BIG_ENDIAN__
static const CFStringEncoding kMediumStringEncoding = kCFStringEncodingUTF16BE;
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32BE;
#elif defined(__LITTLE_ENDIAN__)
static const CFStringEncoding kMediumStringEncoding = kCFStringEncodingUTF16LE;
static const CFStringEncoding kWideStringEncoding = kCFStringEncodingUTF32LE;
#endif  // __LITTLE_ENDIAN__

}  // namespace

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
  return STLStringToSTLStringWithEncodingsT<std::wstring, std::string>(
      wide, kWideStringEncoding, kNarrowStringEncoding);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(std::string_view utf8) {
  return STLStringToSTLStringWithEncodingsT<std::string_view, std::wstring>(
      utf8, kNarrowStringEncoding, kWideStringEncoding);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(std::string_view native_mb) {
  return SysUTF8ToWide(native_mb);
}

ScopedCFTypeRef<CFStringRef> SysUTF8ToCFStringRef(std::string_view utf8) {
  return StringViewToCFStringWithEncodingsT(utf8, kNarrowStringEncoding);
}

ScopedCFTypeRef<CFStringRef> SysUTF16ToCFStringRef(const std::u16string& utf16) {
  return StringToCFStringWithEncodingsT(utf16, kMediumStringEncoding);
}

NSString* SysUTF8ToNSString(std::string_view utf8) {
  return CFBridgingRelease(CFRetain(SysUTF8ToCFStringRef(utf8)));
}

NSString* SysUTF16ToNSString(const std::u16string& utf16) {
  return CFBridgingRelease(CFRetain(SysUTF16ToCFStringRef(utf16)));
}

std::string SysCFStringRefToUTF8(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::string>(ref,
                                                       kNarrowStringEncoding);
}

std::u16string SysCFStringRefToUTF16(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::u16string>(
      ref, kMediumStringEncoding);
}

std::string SysNSStringToUTF8(NSString* nsstring) {
  if (!nsstring)
    return std::string();
  return SysCFStringRefToUTF8(reinterpret_cast<CFStringRef>(nsstring));
}

std::u16string SysNSStringToUTF16(NSString* nsstring) {
  if (!nsstring)
    return std::u16string();
  return SysCFStringRefToUTF16(reinterpret_cast<CFStringRef>(nsstring));
}

#endif  // defined(OS_APPLE) && defined(__clang__)

static std::string main_exe_path = "UNKNOWN";

bool GetExecutablePath(std::string* path) {
  char exe_path[PATH_MAX];
  uint32_t size = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &size) == 0) {
    char link_path[PATH_MAX];
    if (realpath(exe_path, link_path)) {
      *path = link_path;
      return true;
    } else {
      PLOG(WARNING) << "Internal error: realpath failed";
    }
  } else {
    PLOG(WARNING) << "Internal error: _NSGetExecutablePath failed";
  }
  *path = main_exe_path;
  return false;
}

void SetExecutablePath(const std::string& exe_path) {
  main_exe_path = exe_path;

  std::string new_exe_path;
  GetExecutablePath(&new_exe_path);
  absl::flags_internal::SetProgramInvocationName(new_exe_path);
}

#endif  // __APPLE__
