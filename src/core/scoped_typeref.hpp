// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_SCOPED_TYPEREF_H
#define CORE_SCOPED_TYPEREF_H

#include "core/check.hpp"
#include "core/compiler_specific.hpp"
#include "core/scoped_policy.hpp"

// ScopedTypeRef<> is patterned after std::unique_ptr<>, but maintains ownership
// of a reference to any type that is maintained by Retain and Release methods.
//
// The Traits structure must provide the Retain and Release methods for type T.
// A default ScopedTypeRefTraits is used but not defined, and should be defined
// for each type to use this interface. For example, an appropriate definition
// of ScopedTypeRefTraits for CGLContextObj would be:
//
//   template<>
//   struct ScopedTypeRefTraits<CGLContextObj> {
//     static CGLContextObj InvalidValue() { return nullptr; }
//     static CGLContextObj Retain(CGLContextObj object) {
//       CGLContextRetain(object);
//       return object;
//     }
//     static void Release(CGLContextObj object) { CGLContextRelease(object); }
//   };
//
// For the many types that have pass-by-pointer create functions, the function
// InitializeInto() is provided to allow direct initialization and assumption
// of ownership of the object. For example, continuing to use the above
// CGLContextObj specialization:
//
//   ScopedTypeRef<CGLContextObj> context;
//   CGLCreateContext(pixel_format, share_group, context.InitializeInto());
//
// For initialization with an existing object, the caller may specify whether
// the ScopedTypeRef<> being initialized is assuming the caller's existing
// ownership of the object (and should not call Retain in initialization) or if
// it should not assume this ownership and must create its own (by calling
// Retain in initialization). This behavior is based on the |policy| parameter,
// with |ASSUME| for the former and |RETAIN| for the latter. The default policy
// is to |ASSUME|.

template <typename T>
struct ScopedTypeRefTraits;

template <typename T, typename Traits = ScopedTypeRefTraits<T>>
class ScopedTypeRef {
 public:
  using element_type = T;

  explicit constexpr ScopedTypeRef(
      element_type object = Traits::InvalidValue(),
      scoped_policy::OwnershipPolicy policy = scoped_policy::ASSUME)
      : object_(object) {
    if (object_ && policy == scoped_policy::RETAIN)
      object_ = Traits::Retain(object_);
  }

  ScopedTypeRef(const ScopedTypeRef<T, Traits>& that) : object_(that.object_) {
    if (object_)
      object_ = Traits::Retain(object_);
  }

  // This allows passing an object to a function that takes its superclass.
  template <typename R, typename RTraits>
  explicit ScopedTypeRef(const ScopedTypeRef<R, RTraits>& that_as_subclass)
      : object_(that_as_subclass.get()) {
    if (object_)
      object_ = Traits::Retain(object_);
  }

  ScopedTypeRef(ScopedTypeRef<T, Traits>&& that) : object_(that.object_) {
    that.object_ = Traits::InvalidValue();
  }

  ~ScopedTypeRef() {
    if (object_)
      Traits::Release(object_);
  }

  ScopedTypeRef& operator=(const ScopedTypeRef<T, Traits>& that) {
    reset(that.get(), scoped_policy::RETAIN);
    return *this;
  }

  // This is to be used only to take ownership of objects that are created
  // by pass-by-pointer create functions. To enforce this, require that the
  // object be reset to NULL before this may be used.
  WARN_UNUSED_RESULT element_type* InitializeInto() {
    DCHECK(!object_);
    return &object_;
  }

  void reset(const ScopedTypeRef<T, Traits>& that) {
    reset(that.get(), scoped_policy::RETAIN);
  }

  void reset(element_type object = Traits::InvalidValue(),
             scoped_policy::OwnershipPolicy policy = scoped_policy::ASSUME) {
    if (object && policy == scoped_policy::RETAIN)
      object = Traits::Retain(object);
    if (object_)
      Traits::Release(object_);
    object_ = object;
  }

  bool operator==(const element_type& that) const { return object_ == that; }

  bool operator!=(const element_type& that) const { return object_ != that; }

  operator element_type() const { return object_; }

  element_type get() const { return object_; }

  void swap(ScopedTypeRef& that) {
    element_type temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // ScopedTypeRef<>::release() is like std::unique_ptr<>::release.  It is NOT
  // a wrapper for Release().  To force a ScopedTypeRef<> object to call
  // Release(), use ScopedTypeRef<>::reset().
  WARN_UNUSED_RESULT element_type release() {
    element_type temp = object_;
    object_ = Traits::InvalidValue();
    return temp;
  }

 private:
  element_type object_;
};

#endif  // CORE_SCOPED_TYPEREF_H
