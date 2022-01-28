// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#ifdef __APPLE__

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>

#include <mach/mach_time.h>
#include <stdint.h>

#include "core/logging.hpp"
#include "core/foundation_util.hpp"

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

uint64_t GetMonotonicTime() {
  uint64_t now = mach_absolute_time();
  return MachTimeToNanoseconds(now);
}

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
      NULL, reinterpret_cast<const UInt8*>(in.data()),
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
    absl::string_view in,
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
std::wstring SysUTF8ToWide(absl::string_view utf8) {
  return STLStringToSTLStringWithEncodingsT<absl::string_view, std::wstring>(
      utf8, kNarrowStringEncoding, kWideStringEncoding);
}

std::string SysWideToNativeMB(const std::wstring& wide) {
  return SysWideToUTF8(wide);
}

std::wstring SysNativeMBToWide(absl::string_view native_mb) {
  return SysUTF8ToWide(native_mb);
}

ScopedCFTypeRef<CFStringRef> SysUTF8ToCFStringRef(absl::string_view utf8) {
  return StringViewToCFStringWithEncodingsT(utf8, kNarrowStringEncoding);
}

ScopedCFTypeRef<CFStringRef> SysUTF16ToCFStringRef(const std::u16string& utf16) {
  return StringToCFStringWithEncodingsT(utf16, kMediumStringEncoding);
}

NSString* SysUTF8ToNSString(absl::string_view utf8) {
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



#endif  // __APPLE__
