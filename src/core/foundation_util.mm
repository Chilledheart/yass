// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/foundation_util.hpp"

#ifdef __APPLE__

#include <stdint.h>
#include "core/checked_math.hpp"
#include "core/cxx17_backports.hpp"
#include "core/logging.hpp"
#include "core/safe_conversions.hpp"
#include "core/scoped_cftyperef.hpp"
#include "core/utils.hpp"

#if !defined(OS_IOS)
#import <AppKit/AppKit.h>
#endif

extern "C" {
CFTypeID SecKeyGetTypeID();
#if !defined(OS_IOS)
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

#if !defined(OS_IOS)
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
  if (cf_val == nullptr) { \
    return nullptr; \
  } \
  if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) { \
    return (TypeCF##Ref)(cf_val); \
  } \
  return nullptr; \
} \
\
template<> TypeCF##Ref \
CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
  TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val); \
  DCHECK(cf_val == nullptr || rv); \
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

#if defined(OS_IOS)
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
  if (cf_val == nullptr) {
    return nullptr;
  }
  if (CFGetTypeID(cf_val) == CTFontGetTypeID()) {
    return (CTFontRef)(cf_val);
  }

  if (!_CFIsObjC(CTFontGetTypeID(), cf_val))
    return nullptr;

  id<NSObject> ns_val = reinterpret_cast<id>(const_cast<void*>(cf_val));
  if ([ns_val isKindOfClass:[NSFont class]]) {
    return (CTFontRef)(cf_val);
  }
  return nullptr;
}

template<> CTFontRef
CFCastStrict<CTFontRef>(const CFTypeRef& cf_val) {
  CTFontRef rv = CFCast<CTFontRef>(cf_val);
  DCHECK(cf_val == nullptr || rv);
  return rv;
}
#endif

#if !defined(OS_IOS)
CF_CAST_DEFN(SecACL)
CF_CAST_DEFN(SecCertificate)
CF_CAST_DEFN(SecKey)
CF_CAST_DEFN(SecPolicy)
CF_CAST_DEFN(SecTrustedApplication)
#endif

#undef CF_CAST_DEFN

std::string GetValueFromDictionaryErrorMessage(
    CFStringRef key, const std::string& expected_type, CFTypeRef value) {
  ScopedCFTypeRef<CFStringRef> actual_type_ref(
      CFCopyTypeIDDescription(CFGetTypeID(value)));
  return "Expected value for key " +
      SysCFStringRefToUTF8(key) +
      " to be " +
      expected_type +
      " but it was " +
      SysCFStringRefToUTF8(actual_type_ref) +
      " instead";
}

std::ostream& operator<<(std::ostream& o, const CFStringRef string) {
  return o << SysCFStringRefToUTF8(string);
}

std::ostream& operator<<(std::ostream& o, const CFErrorRef err) {
  ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(err));
  ScopedCFTypeRef<CFDictionaryRef> user_info(CFErrorCopyUserInfo(err));
  CFStringRef errorDesc = nullptr;
  if (user_info.get()) {
    errorDesc = reinterpret_cast<CFStringRef>(
        CFDictionaryGetValue(user_info.get(), kCFErrorDescriptionKey));
  }
  o << "Code: " << CFErrorGetCode(err) << " Domain: " << CFErrorGetDomain(err)
    << " Desc: " << desc.get();
  if(errorDesc) {
    o << "(" << errorDesc << ")";
  }
  return o;
}

std::ostream& operator<<(std::ostream& o, CFRange range) {
  return o << NSStringFromRange(NSMakeRange(range.location, range.length));
}

std::ostream& operator<<(std::ostream& o, id obj) {
  return obj ? o << [obj description].UTF8String : o << "(nil)";
}

std::ostream& operator<<(std::ostream& o, NSRange range) {
  return o << NSStringFromRange(range);
}

std::ostream& operator<<(std::ostream& o, SEL selector) {
  return o << NSStringFromSelector(selector);
}

#if !defined(OS_IOS)
std::ostream& operator<<(std::ostream& o, NSPoint point) {
  return o << NSStringFromPoint(point);
}
std::ostream& operator<<(std::ostream& o, NSRect rect) {
  return o << NSStringFromRect(rect);
}
std::ostream& operator<<(std::ostream& o, NSSize size) {
  return o << NSStringFromSize(size);
}
#endif

#endif // __APPLE__
