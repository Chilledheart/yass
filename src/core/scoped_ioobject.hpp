// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_SCOPED_IOOBJECT_H
#define CORE_SCOPED_IOOBJECT_H

#include "core/scoped_typeref.hpp"

namespace internal {

template <typename IOT>
struct ScopedIOObjectTraits {
  static IOT InvalidValue() { return IO_OBJECT_NULL; }
  static IOT Retain(IOT iot) {
    IOObjectRetain(iot);
    return iot;
  }
  static void Release(IOT iot) { IOObjectRelease(iot); }
};

}  // namespace internal

// Just like ScopedCFTypeRef but for io_object_t and subclasses.
template <typename IOT>
using ScopedIOObject = ScopedTypeRef<IOT, internal::ScopedIOObjectTraits<IOT>>;

#endif  // CORE_SCOPED_IOOBJECT_H
