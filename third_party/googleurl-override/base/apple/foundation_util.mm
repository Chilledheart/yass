// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>
#include <vector>

#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

#if !defined(OS_IOS)
#import <AppKit/AppKit.h>
#endif

extern "C" {
CFTypeID SecKeyGetTypeID();
}  // extern "C"

namespace gurl_base::apple {

std::string PathForFrameworkBundleResource(const char* resource_name) {
  NSBundle* bundle = [NSBundle mainBundle];
  NSURL* resource_url = [bundle URLForResource:@(resource_name)
                                 withExtension:nil];
  const char* path = nullptr;
  if (resource_url && resource_url.fileURL) {
    path = resource_url.path.fileSystemRepresentation;
  }
  return path ? path : std::string();
}

#define TYPE_NAME_FOR_CF_TYPE_DEFN(TypeCF)     \
  std::string TypeNameForCFType(TypeCF##Ref) { \
    return #TypeCF;                            \
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

#if !BUILDFLAG(IS_IOS)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecAccessControl)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecCertificate)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecKey)
TYPE_NAME_FOR_CF_TYPE_DEFN(SecPolicy)
#endif

#undef TYPE_NAME_FOR_CF_TYPE_DEFN

#define CF_CAST_DEFN(TypeCF)                                       \
  template <>                                                      \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val) {       \
    if (cf_val == NULL) {                                          \
      return NULL;                                                 \
    }                                                              \
    if (CFGetTypeID(cf_val) == TypeCF##GetTypeID()) {              \
      return (TypeCF##Ref)(cf_val);                                \
    }                                                              \
    return NULL;                                                   \
  }                                                                \
                                                                   \
  template <>                                                      \
  TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val) { \
    TypeCF##Ref rv = CFCast<TypeCF##Ref>(cf_val);                  \
    GURL_DCHECK(cf_val == NULL || rv);                             \
    return rv;                                                     \
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

CF_CAST_DEFN(CTFont)
CF_CAST_DEFN(CTFontDescriptor)
CF_CAST_DEFN(CTRun)

CF_CAST_DEFN(SecCertificate)

#if !BUILDFLAG(IS_IOS)
CF_CAST_DEFN(SecAccessControl)
CF_CAST_DEFN(SecKey)
CF_CAST_DEFN(SecPolicy)
#endif

#undef CF_CAST_DEFN

std::string GetValueFromDictionaryErrorMessage(
    CFStringRef key, const std::string& expected_type, CFTypeRef value) {
  ScopedCFTypeRef<CFStringRef> actual_type_ref(
      CFCopyTypeIDDescription(CFGetTypeID(value)));
  return "Expected value for key " + SysCFStringRefToUTF8(key) + " to be " +
      expected_type + " but it was " +
      SysCFStringRefToUTF8(actual_type_ref) + " instead";
}

bool CFRangeToNSRange(CFRange range, NSRange* range_out) {
  NSUInteger end;
  if (IsValueInRangeForNumericType<NSUInteger>(range.location) &&
      IsValueInRangeForNumericType<NSUInteger>(range.length) &&
      CheckAdd(range.location, range.length).AssignIfValid(&end) &&
      IsValueInRangeForNumericType<NSUInteger>(end)) {
    *range_out = NSMakeRange(static_cast<NSUInteger>(range.location),
                             static_cast<NSUInteger>(range.length));
    return true;
  }
  return false;
}

}  // namespace gurl_base::apple

std::ostream& operator<<(std::ostream& o, const CFStringRef string) {
  return o << gurl_base::SysCFStringRefToUTF8(string);
}

std::ostream& operator<<(std::ostream& o, const CFErrorRef err) {
  gurl_base::apple::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(err));
  gurl_base::apple::ScopedCFTypeRef<CFDictionaryRef> user_info(
      CFErrorCopyUserInfo(err));
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
