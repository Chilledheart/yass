// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef CORE_FOUNDATION_UTIL_H
#define CORE_FOUNDATION_UTIL_H

#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "core/scoped_cftyperef.hpp"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#else  // __OBJC__
#include <CoreFoundation/CoreFoundation.h>
class NSBundle;
class NSString;
#endif  // __OBJC__

#if defined(OS_IOS)
#include <CoreText/CoreText.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

// Adapted from NSPathUtilities.h and NSObjCRuntime.h.
#if __LP64__ || NS_BUILD_32_LIKE_64
enum NSSearchPathDirectory : unsigned long;
typedef unsigned long NSSearchPathDomainMask;
#else
enum NSSearchPathDirectory : unsigned int;
typedef unsigned int NSSearchPathDomainMask;
#endif

typedef struct CF_BRIDGED_TYPE(id) __SecCertificate* SecCertificateRef;
typedef struct CF_BRIDGED_TYPE(id) __SecKey* SecKeyRef;
typedef struct CF_BRIDGED_TYPE(id) __SecPolicy* SecPolicyRef;

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
// CFNumberRef some_number = CFCast<CFNumberRef>(
//     CFArrayGetValueAtIndex(array, index));
//
// CFTypeRef hello = CFSTR("hello world");
// CFStringRef some_string = CFCastStrict<CFStringRef>(hello);

template <typename T>
T CFCast(const CFTypeRef& cf_val);

template <typename T>
T CFCastStrict(const CFTypeRef& cf_val);

#define CF_CAST_DECL(TypeCF)                                \
  template <>                                               \
  TypeCF##Ref CFCast<TypeCF##Ref>(const CFTypeRef& cf_val); \
                                                            \
  template <>                                               \
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
// NSString* version = ObjCCast<NSString>(
//     [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
//
// NSString* str = ObjCCastStrict<NSString>(
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
  DCHECK(objc_val == nil || rv);
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
    DLOG(WARNING) << GetValueFromDictionaryErrorMessage(key, expected_type,
                                                        value);
  }

  return value_specific;
}

#if defined(__OBJC__)
// Converts |range| to an NSRange, returning the new range in |range_out|.
// Returns true if conversion was successful, false if the values of |range|
// could not be converted to NSUIntegers.
WARN_UNUSED_RESULT bool CFRangeToNSRange(CFRange range, NSRange* range_out);
#endif  // defined(__OBJC__)

// Stream operations for CFTypes. They can be used with NSTypes as well
// by using the NSToCFCast methods above.
// e.g. LOG(INFO) << NSToCFCast(@"foo");
// Operator << can not be overloaded for ObjectiveC types as the compiler
// can not distinguish between overloads for id with overloads for void*.
extern std::ostream& operator<<(std::ostream& o, const CFErrorRef err);
extern std::ostream& operator<<(std::ostream& o, const CFStringRef str);
extern std::ostream& operator<<(std::ostream& o, CFRange);

#if defined(__OBJC__)
extern std::ostream& operator<<(std::ostream& o, id);
extern std::ostream& operator<<(std::ostream& o, NSRange);
extern std::ostream& operator<<(std::ostream& o, SEL);

#if !defined(OS_IOS)
extern std::ostream& operator<<(std::ostream& o, NSPoint);
extern std::ostream& operator<<(std::ostream& o, NSRect);
extern std::ostream& operator<<(std::ostream& o, NSSize);
#endif

#endif  // __OBJC__

#endif  // CORE_FOUNDATION_UTIL_H
