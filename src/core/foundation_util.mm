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
