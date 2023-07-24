//===-- umodti3.c - Implement __umodti3 -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements __umodti3 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#define CRT_HAS_128BIT
#ifdef CRT_HAS_128BIT

typedef int ti_int __attribute__((mode(TI)));
typedef unsigned tu_int __attribute__((mode(TI)));
extern tu_int __umodti3(tu_int a, tu_int b);
extern int __clzti2(ti_int a);
extern tu_int __udivmodti4(tu_int a, tu_int b, tu_int *rem);

// Returns: a % b

tu_int __umodti3(tu_int a, tu_int b) {
  tu_int r;
  __udivmodti4(a, b, &r);
  return r;
}

#endif // CRT_HAS_128BIT
