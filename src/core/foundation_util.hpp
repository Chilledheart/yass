// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */
#ifndef CORE_FOUNDATION_UTIL_H
#define CORE_FOUNDATION_UTIL_H

#include "core/compiler_specific.hpp"
#include "core/logging.hpp"
#include "core/scoped_cftyperef.hpp"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
@class NSFont;
@class UIFont;
#else  // __OBJC__
#include <CoreFoundation/CoreFoundation.h>
class NSBundle;
class NSFont;
class NSString;
class UIFont;
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
  OBJC_CPP_CLASS_DECL(TypeNS)              \
                                           \
  TypeNS* CFToNSCast(TypeCF##Ref cf_val);  \
  TypeCF##Ref NSToCFCast(TypeNS* ns_val);

#define CF_TO_NS_MUTABLE_CAST_DECL(name)                    \
  CF_TO_NS_CAST_DECL(CF##name, NS##name)                    \
  OBJC_CPP_CLASS_DECL(NSMutable##name)                      \
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

#if defined(OS_IOS)
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
