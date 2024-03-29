// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_FOUNDATION_UTIL_H_
#define BASE_APPLE_FOUNDATION_UTIL_H_

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <Security/Security.h>

#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/base_export.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class NSFont;
@class UIFont;
#endif  // __OBJC__

namespace gurl_base::apple {

// Returns the path to a resource within the framework bundle.
BASE_EXPORT std::string PathForFrameworkBundleResource(const char* resource_name);

#define TYPE_NAME_FOR_CF_TYPE_DECL(TypeCF) \
  BASE_EXPORT std::string TypeNameForCFType(TypeCF##Ref)

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

TYPE_NAME_FOR_CF_TYPE_DECL(SecAccessControl);
TYPE_NAME_FOR_CF_TYPE_DECL(SecCertificate);
TYPE_NAME_FOR_CF_TYPE_DECL(SecKey);
TYPE_NAME_FOR_CF_TYPE_DECL(SecPolicy);

#undef TYPE_NAME_FOR_CF_TYPE_DECL

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
// CFNumberRef some_number = gurl_base::apple::CFCast<CFNumberRef>(
//     CFArrayGetValueAtIndex(array, index));
//
// CFTypeRef hello = CFSTR("hello world");
// CFStringRef some_string = gurl_base::apple::CFCastStrict<CFStringRef>(hello);

template <typename T>
T CFCast(const CFTypeRef& cf_val);

template <typename T>
T CFCastStrict(const CFTypeRef& cf_val);

#define CF_CAST_DECL(TypeCF)                                            \
  template <>                                                           \
  BASE_EXPORT TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val); \
                                                                        \
  template <>                                                           \
  BASE_EXPORT TypeCF##Ref CFCastStrict<TypeCF##Ref>(const CFTypeRef& cf_val)

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

CF_CAST_DECL(SecAccessControl);
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
// NSString* version = gurl_base::apple::ObjCCast<NSString>(
//     [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
//
// NSString* str = gurl_base::apple::ObjCCastStrict<NSString>(
//     [ns_arr_of_ns_strs objectAtIndex:0]);
template <typename T>
T* ObjCCast(id objc_val) {
  if ([objc_val isKindOfClass:[T class]]) {
    return reinterpret_cast<T*>(objc_val);
  }
  return nil;
}

template <typename T>
T* ObjCCastStrict(id objc_val) {
  T* rv = ObjCCast<T>(objc_val);
  GURL_DCHECK(objc_val == nil || rv);
  return rv;
}

#endif  // defined(__OBJC__)

// Helper function for GetValueFromDictionary to create the error message
// that appears when a type mismatch is encountered.
std::string GetValueFromDictionaryErrorMessage(CFStringRef key,
                                               const std::string& expected_type,
                                               CFTypeRef value);

// Utility function to pull out a value from a dictionary, check its type, and
// return it. Returns NULL if the key is not present or of the wrong type.
template <typename T>
T GetValueFromDictionary(CFDictionaryRef dict, CFStringRef key) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  T value_specific = CFCast<T>(value);

  if (value && !value_specific) {
    std::string expected_type = TypeNameForCFType(value_specific);
    GURL_DLOG(WARNING) << GetValueFromDictionaryErrorMessage(key, expected_type,
                                                        value);
  }

  return value_specific;
}

#if defined(__OBJC__)
// Converts |range| to an NSRange, returning the new range in |range_out|.
// Returns true if conversion was successful, false if the values of |range|
// could not be converted to NSUIntegers.
[[nodiscard]] BASE_EXPORT bool CFRangeToNSRange(CFRange range, NSRange* range_out);
#endif  // defined(__OBJC__)

}  // namespace gurl_base::apple

// Stream operations for CFTypes. They can be used with Objective-C types as
// well by using the casting methods in base/apple/bridging.h.
//
// For example: GURL_LOG(INFO) << gurl_base::apple::NSToCFPtrCast(@"foo");
//
// operator<<() can not be overloaded for Objective-C types as the compiler
// cannot distinguish between overloads for id with overloads for void*.
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o,
                                            const CFErrorRef err);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o,
                                            const CFStringRef str);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, CFRange);

#if defined(__OBJC__)
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, id);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSRange);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, SEL);

#if BUILDFLAG(IS_MAC)
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSPoint);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSRect);
BASE_EXPORT extern std::ostream& operator<<(std::ostream& o, NSSize);
#endif  // IS_MAC

#endif  // __OBJC__

#endif  // BASE_APPLE_FOUNDATION_UTIL_H_
