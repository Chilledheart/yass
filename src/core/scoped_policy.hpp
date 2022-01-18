// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_SCOPED_POLICY_H
#define CORE_SCOPED_POLICY_H

namespace scoped_policy {

// Defines the ownership policy for a scoped object.
enum OwnershipPolicy {
  // The scoped object takes ownership of an object by taking over an existing
  // ownership claim.
  ASSUME,

  // The scoped object will retain the object and any initial ownership is
  // not changed.
  RETAIN
};

}  // namespace scoped_policy

#endif  // CORE_SCOPED_POLICY_H
