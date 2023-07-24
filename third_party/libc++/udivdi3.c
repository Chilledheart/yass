//===-- udivdi3.c - Implement __udivdi3 -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements __udivdi3 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include <stdint.h>

typedef int64_t di_int;
typedef uint64_t du_int;

typedef du_int fixuint_t;
typedef di_int fixint_t;

extern du_int __udivdi3(du_int a, du_int b);

#include "int_div_impl.inc"

// Returns: a / b

du_int __udivdi3(du_int a, du_int b) {
  return __udivXi3(a, b);
}
