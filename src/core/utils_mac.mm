// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utils.hpp"

#ifdef __APPLE__

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>

#include <mach/mach_time.h>
#include <stdint.h>

#include "core/logging.hpp"

typedef struct CF_BRIDGED_TYPE(id) __SecCertificate* SecCertificateRef;
typedef struct CF_BRIDGED_TYPE(id) __SecKey* SecKeyRef;
typedef struct CF_BRIDGED_TYPE(id) __SecPolicy* SecPolicyRef;

#define TYPE_NAME_FOR_CF_TYPE_DECL(TypeCF) \
  std::string TypeNameForCFType(TypeCF##Ref)

TYPE_NAME_FOR_CF_TYPE_DECL(CFArray);
TYPE_NAME_FOR_CF_TYPE_DECL(CFBag);
TYPE_NAME_FOR_CF_TYPE_DECL(CFBoolean);
TYPE_NAME_FOR_CF_TYPE_DECL(CFData);
TYPE_NAME_FOR_CF_TYPE_DECL(CFDate);
TYPE_NAME_FOR_CF_TYPE_DECL(CFDictionary);
TYPE_NAME_FOR_CF_TYPE_DECL(CFNull);
TYPE_NAME_FOR_CF_TYPE_DECL(CFNumber);
TYPE_NAME_FOR_CF_TYPE_DECL(CFSet);
TYPE_NAME_FOR_CF_TYPE_DECL(CFString);
TYPE_NAME_FOR_CF_TYPE_DECL(CFURL);
TYPE_NAME_FOR_CF_TYPE_DECL(CFUUID);

TYPE_NAME_FOR_CF_TYPE_DECL(CGColor);

TYPE_NAME_FOR_CF_TYPE_DECL(CTFont);
TYPE_NAME_FOR_CF_TYPE_DECL(CTRun);

TYPE_NAME_FOR_CF_TYPE_DECL(SecCertificate);
TYPE_NAME_FOR_CF_TYPE_DECL(SecKey);
TYPE_NAME_FOR_CF_TYPE_DECL(SecPolicy);

#undef TYPE_NAME_FOR_CF_TYPE_DECL

// Retain/release calls for memory management in C++.
void NSObjectRetain(void* obj);
void NSObjectRelease(void* obj);

#if !defined(__OBJC__)
#define OBJC_CPP_CLASS_DECL(x) class x;
#else  // __OBJC__
#define OBJC_CPP_CLASS_DECL(x)
#endif  // __OBJC__

// Convert toll-free bridged CFTypes to NSTypes and vice-versa. This does not
// autorelease |cf_val|. This is useful for the case where there is a CFType in
// a call that expects an NSType and the compiler is complaining about const
// casting problems.
// The calls are used like this:
// NSString *foo = CFToNSCast(CFSTR("Hello"));
// CFStringRef foo2 = NSToCFCast(@"Hello");
// The macro magic below is to enforce safe casting. It could possibly have
// been done using template function specialization, but template function
// specialization doesn't always work intuitively,
// (http://www.gotw.ca/publications/mill17.htm) so the trusty combination
// of macros and function overloading is used instead.

#define CF_TO_NS_CAST_DECL(TypeCF, TypeNS) \
OBJC_CPP_CLASS_DECL(TypeNS) \
\
TypeNS* CFToNSCast(TypeCF##Ref cf_val); \
TypeCF##Ref NSToCFCast(TypeNS* ns_val);

#define CF_TO_NS_MUTABLE_CAST_DECL(name) \
CF_TO_NS_CAST_DECL(CF##name, NS##name) \
OBJC_CPP_CLASS_DECL(NSMutable##name) \
\
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val); \
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val);

// List of toll-free bridged types taken from:
// http://www.cocoadev.com/index.pl?TollFreeBridged

CF_TO_NS_MUTABLE_CAST_DECL(Array)
CF_TO_NS_MUTABLE_CAST_DECL(AttributedString)
CF_TO_NS_CAST_DECL(CFCalendar, NSCalendar)
CF_TO_NS_MUTABLE_CAST_DECL(CharacterSet)
CF_TO_NS_MUTABLE_CAST_DECL(Data)
CF_TO_NS_CAST_DECL(CFDate, NSDate)
CF_TO_NS_MUTABLE_CAST_DECL(Dictionary)
CF_TO_NS_CAST_DECL(CFError, NSError)
CF_TO_NS_CAST_DECL(CFLocale, NSLocale)
CF_TO_NS_CAST_DECL(CFNumber, NSNumber)
CF_TO_NS_CAST_DECL(CFRunLoopTimer, NSTimer)
CF_TO_NS_CAST_DECL(CFTimeZone, NSTimeZone)
CF_TO_NS_MUTABLE_CAST_DECL(Set)
CF_TO_NS_CAST_DECL(CFReadStream, NSInputStream)
CF_TO_NS_CAST_DECL(CFWriteStream, NSOutputStream)
CF_TO_NS_MUTABLE_CAST_DECL(String)
CF_TO_NS_CAST_DECL(CFURL, NSURL)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
CF_TO_NS_CAST_DECL(CTFont, UIFont)
#else
CF_TO_NS_CAST_DECL(CTFont, NSFont)
#endif

#undef CF_TO_NS_CAST_DECL
#undef CF_TO_NS_MUTABLE_CAST_DECL
#undef OBJC_CPP_CLASS_DECL

// CFCast<>() and CFCastStrict<>() cast a basic CFTypeRef to a more
// specific CoreFoundation type. The compatibility of the passed
// object is found by comparing its opaque type against the
// requested type identifier. If the supplied object is not
// compatible with the requested return type, CFCast<>() returns
// NULL and CFCastStrict<>() will DCHECK. Providing a NULL pointer
// to either variant results in NULL being returned without
// triggering any DCHECK.
//
// Example usage:
// CFNumberRef some_number = base::mac::CFCast<CFNumberRef>(
//     CFArrayGetValueAtIndex(array, index));
//
// CFTypeRef hello = CFSTR("hello world");
// CFStringRef some_string = base::mac::CFCastStrict<CFStringRef>(hello);

template<typename T>
T CFCast(const CFTypeRef& cf_val);

template<typename T>
T CFCastStrict(const CFTypeRef& cf_val);

#define CF_CAST_DECL(TypeCF)                                            \
  template <>                                                           \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val); \
                                                                        \
  template <>                                                           \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val)

CF_CAST_DECL(CFArray);
CF_CAST_DECL(CFBag);
CF_CAST_DECL(CFBoolean);
CF_CAST_DECL(CFData);
CF_CAST_DECL(CFDate);
CF_CAST_DECL(CFDictionary);
CF_CAST_DECL(CFNull);
CF_CAST_DECL(CFNumber);
CF_CAST_DECL(CFSet);
CF_CAST_DECL(CFString);
CF_CAST_DECL(CFURL);
CF_CAST_DECL(CFUUID);

CF_CAST_DECL(CGColor);

CF_CAST_DECL(CTFont);
CF_CAST_DECL(CTFontDescriptor);
CF_CAST_DECL(CTRun);

CF_CAST_DECL(SecCertificate);
CF_CAST_DECL(SecKey);
CF_CAST_DECL(SecPolicy);

#undef CF_CAST_DECL

#if defined(__OBJC__)

// ObjCCast<>() and ObjCCastStrict<>() cast a basic id to a more
// specific (NSObject-derived) type. The compatibility of the passed
// object is found by checking if it's a kind of the requested type
// identifier. If the supplied object is not compatible with the
// requested return type, ObjCCast<>() returns nil and
// ObjCCastStrict<>() will DCHECK. Providing a nil pointer to either
// variant results in nil being returned without triggering any DCHECK.
//
// The strict variant is useful when retrieving a value from a
// collection which only has values of a specific type, e.g. an
// NSArray of NSStrings. The non-strict variant is useful when
// retrieving values from data that you can't fully control. For
// example, a plist read from disk may be beyond your exclusive
// control, so you'd only want to check that the values you retrieve
// from it are of the expected types, but not crash if they're not.
//
// Example usage:
// NSString* version = base::mac::ObjCCast<NSString>(
//     [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
//
// NSString* str = base::mac::ObjCCastStrict<NSString>(
//     [ns_arr_of_ns_strs objectAtIndex:0]);
template<typename T>
T* ObjCCast(id objc_val) {
  if ([objc_val isKindOfClass:[T class]]) {
    return reinterpret_cast<T*>(objc_val);
  }
  return nil;
}

template<typename T>
T* ObjCCastStrict(id objc_val) {
  T* rv = ObjCCast<T>(objc_val);
  DCHECK(objc_val == nil || rv);
  return rv;
}

#endif  // defined(__OBJC__)

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
                                       NULL,   // buffer
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
                               NULL);  // usedBufLen
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

  CFStringRef cfstring(CFStringCreateWithBytesNoCopy(
      NULL,
      reinterpret_cast<const UInt8*>(in.data()),
      in_length * sizeof(typename InStringType::value_type),
      in_encoding,
      false,
      kCFAllocatorNull));
  if (!cfstring)
    return OutStringType();

  return CFStringToSTLStringWithEncodingT<OutStringType>(cfstring,
                                                         out_encoding);
}

// Given a std::string_view|in| with an encoding specified by |in_encoding|, return
// it as a CFStringRef.  Returns NULL on failure.
template <typename CharT>
static CFStringRef StringToCFStringWithEncodingsT(
    const std::basic_string<CharT> &in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  CFStringRef ret;
  if (in_length == 0) {
    ret = CFSTR("");
  } else {
    ret = CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const uint8_t*>(in.data()),
      in_length * sizeof(CharT), in_encoding, false);
  }

  CFRetain(ret);
  return ret;
}

// Given a std::string_view|in| with an encoding specified by |in_encoding|, return
// it as a CFStringRef.  Returns NULL on failure.
static CFStringRef StringViewToCFStringWithEncodingsT(
    absl::string_view in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  CFStringRef ret;
  if (in_length == 0) {
    ret = CFSTR("");
  } else {
    ret = CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const uint8_t*>(in.data()),
      in_length * sizeof(char), in_encoding, false);
  }

  CFRetain(ret);
  return ret;
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

CFStringRef SysUTF8ToCFStringRef(absl::string_view utf8) {
  return StringViewToCFStringWithEncodingsT(utf8, kNarrowStringEncoding);
}

CFStringRef SysUTF16ToCFStringRef(const std::u16string& utf16) {
  return StringToCFStringWithEncodingsT(utf16, kMediumStringEncoding);
}

NSString* SysUTF8ToNSString(absl::string_view utf8) {
  return [CFToNSCast(SysUTF8ToCFStringRef(utf8)) autorelease];
}

NSString* SysUTF16ToNSString(const std::u16string& utf16) {
  return [CFToNSCast(SysUTF16ToCFStringRef(utf16)) autorelease];
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

#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#import <AppKit/AppKit.h>
#endif

extern "C" {
CFTypeID SecKeyGetTypeID();
#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
CFTypeID SecACLGetTypeID();
CFTypeID SecTrustedApplicationGetTypeID();
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this SPI declaration.
Boolean _CFIsObjC(CFTypeID typeID, CFTypeRef obj);
#endif
}  // extern "C"

#define TYPE_NAME_FOR_CF_TYPE_DEFN(TypeCF) \
std::string TypeNameForCFType(TypeCF##Ref) { \
  return #TypeCF; \
}

TYPE_NAME_FOR_CF_TYPE_DEFN(CFArray)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFBag)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFBoolean)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFData)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFDate)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFDictionary)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFNull)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFNumber)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFSet)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFString)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFURL)
TYPE_NAME_FOR_CF_TYPE_DEFN(CFUUID)

TYPE_NAME_FOR_CF_TYPE_DEFN(CGColor)

TYPE_NAME_FOR_CF_TYPE_DEFN(CTFont)
TYPE_NAME_FOR_CF_TYPE_DEFN(CTRun)

#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecCertificate)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecKey)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecPolicy)
#endif

#undef TYPE_NAME_FOR_CF_TYPE_DEFN

void NSObjectRetain(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj retain];
}

void NSObjectRelease(void* obj) {
  id<NSObject> nsobj = static_cast<id<NSObject> >(obj);
  [nsobj release];
}


// Definitions for the corresponding CF_TO_NS_CAST_DECL macros in
// foundation_util.h.
#define CF_TO_NS_CAST_DEFN(TypeCF, TypeNS) \
\
TypeNS* CFToNSCast(TypeCF##Ref cf_val) { \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  TypeNS* ns_val = \
      const_cast<TypeNS*>(reinterpret_cast<const TypeNS*>(cf_val)); \
  return ns_val; \
} \
\
TypeCF##Ref NSToCFCast(TypeNS* ns_val) { \
  TypeCF##Ref cf_val = reinterpret_cast<TypeCF##Ref>(ns_val); \
  DCHECK(!cf_val || TypeCF##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
}

#define CF_TO_NS_MUTABLE_CAST_DEFN(name) \
CF_TO_NS_CAST_DEFN(CF##name, NS##name) \
\
NSMutable##name* CFToNSCast(CFMutable##name##Ref cf_val) { \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  NSMutable##name* ns_val = reinterpret_cast<NSMutable##name*>(cf_val); \
  return ns_val; \
} \
\
CFMutable##name##Ref NSToCFCast(NSMutable##name* ns_val) { \
  CFMutable##name##Ref cf_val = \
      reinterpret_cast<CFMutable##name##Ref>(ns_val); \
  DCHECK(!cf_val || CF##name##GetTypeID() == CFGetTypeID(cf_val)); \
  return cf_val; \
}

CF_TO_NS_MUTABLE_CAST_DEFN(Array)
CF_TO_NS_MUTABLE_CAST_DEFN(AttributedString)
CF_TO_NS_CAST_DEFN(CFCalendar, NSCalendar)
CF_TO_NS_MUTABLE_CAST_DEFN(CharacterSet)
CF_TO_NS_MUTABLE_CAST_DEFN(Data)
CF_TO_NS_CAST_DEFN(CFDate, NSDate)
CF_TO_NS_MUTABLE_CAST_DEFN(Dictionary)
CF_TO_NS_CAST_DEFN(CFError, NSError)
CF_TO_NS_CAST_DEFN(CFLocale, NSLocale)
CF_TO_NS_CAST_DEFN(CFNumber, NSNumber)
CF_TO_NS_CAST_DEFN(CFRunLoopTimer, NSTimer)
CF_TO_NS_CAST_DEFN(CFTimeZone, NSTimeZone)
CF_TO_NS_MUTABLE_CAST_DEFN(Set)
CF_TO_NS_CAST_DEFN(CFReadStream, NSInputStream)
CF_TO_NS_CAST_DEFN(CFWriteStream, NSOutputStream)
CF_TO_NS_MUTABLE_CAST_DEFN(String)
CF_TO_NS_CAST_DEFN(CFURL, NSURL)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
CF_TO_NS_CAST_DEFN(CTFont, UIFont)
#else
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this specialization and replace it with just:
//
// CF_TO_NS_CAST_DEFN(CTFont, NSFont)
NSFont* CFToNSCast(CTFontRef cf_val) {
  NSFont* ns_val =
      const_cast<NSFont*>(reinterpret_cast<const NSFont*>(cf_val));
  DCHECK(!cf_val ||
         CTFontGetTypeID() == CFGetTypeID(cf_val) ||
         (_CFIsObjC(CTFontGetTypeID(), cf_val) &&
          [ns_val isKindOfClass:[NSFont class]]));
  return ns_val;
}

CTFontRef NSToCFCast(NSFont* ns_val) {
  CTFontRef cf_val = reinterpret_cast<CTFontRef>(ns_val);
  DCHECK(!cf_val ||
         CTFontGetTypeID() == CFGetTypeID(cf_val) ||
         [ns_val isKindOfClass:[NSFont class]]);
  return cf_val;
}
#endif

#undef CF_TO_NS_CAST_DEFN
#undef CF_TO_NS_MUTABLE_CAST_DEFN

#define CF_CAST_DEFN(TypeCF) \
template<> TypeCF##Ref \
CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) { \
  if (cf_val == NULL) { \
    return NULL; \
  } \
  if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) { \
    return (TypeCF##Ref)(cf_val); \
  } \
  return NULL; \
} \
\
template<> TypeCF##Ref \
CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
  TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val); \
  DCHECK(cf_val == NULL || rv); \
  return rv; \
}

CF_CAST_DEFN(CFArray)
CF_CAST_DEFN(CFBag)
CF_CAST_DEFN(CFBoolean)
CF_CAST_DEFN(CFData)
CF_CAST_DEFN(CFDate)
CF_CAST_DEFN(CFDictionary)
CF_CAST_DEFN(CFNull)
CF_CAST_DEFN(CFNumber)
CF_CAST_DEFN(CFSet)
CF_CAST_DEFN(CFString)
CF_CAST_DEFN(CFURL)
CF_CAST_DEFN(CFUUID)

CF_CAST_DEFN(CGColor)

CF_CAST_DEFN(CTFontDescriptor)
CF_CAST_DEFN(CTRun)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
CF_CAST_DEFN(CTFont)
#else
// The NSFont/CTFont toll-free bridging is broken before 10.15.
// http://www.openradar.me/15341349 rdar://15341349
//
// TODO(https://crbug.com/1076527): This is fixed in 10.15. When 10.15 is the
// minimum OS for Chromium, remove this specialization and the #if IOS above,
// and rely just on the one CF_CAST_DEFN(CTFont).
template<> CTFontRef
CFCast<CTFontRef>(const CFTypeRef& cf_val) {
  if (cf_val == NULL) {
    return NULL;
  }
  if (CFGetTypeID(cf_val) == CTFontGetTypeID()) {
    return (CTFontRef)(cf_val);
  }

  if (!_CFIsObjC(CTFontGetTypeID(), cf_val))
    return NULL;

  id<NSObject> ns_val = reinterpret_cast<id>(const_cast<void*>(cf_val));
  if ([ns_val isKindOfClass:[NSFont class]]) {
    return (CTFontRef)(cf_val);
  }
  return NULL;
}

template<> CTFontRef
CFCastStrict<CTFontRef>(const CFTypeRef& cf_val) {
  CTFontRef rv = CFCast<CTFontRef>(cf_val);
  DCHECK(cf_val == NULL || rv);
  return rv;
}
#endif

#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
CF_CAST_DEFN(SecACL)
CF_CAST_DEFN(SecCertificate)
CF_CAST_DEFN(SecKey)
CF_CAST_DEFN(SecPolicy)
CF_CAST_DEFN(SecTrustedApplication)
#endif

#undef CF_CAST_DEFN

#endif  // __APPLE__
