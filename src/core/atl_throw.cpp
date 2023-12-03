// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifdef COMPILER_MSVC

#include "core/atl_throw.hpp"

#include <winerror.h>
#include <windows.h>

#include <iterator>

#include "core/compiler_specific.hpp"
#include "core/immediate_crash.hpp"
#include "core/logging.hpp"

// Custom Windows exception code chosen to indicate an out of memory error.
// See https://msdn.microsoft.com/en-us/library/het71c37.aspx.
// "To make sure that you do not define a code that conflicts with an existing
// exception code" ... "The resulting error code should therefore have the
// highest four bits set to hexadecimal E."
// 0xe0000008 was chosen arbitrarily, as 0x00000008 is ERROR_NOT_ENOUGH_MEMORY.
static const DWORD kOomExceptionCode = 0xe0000008;

static void TerminateBecauseOutOfMemory() {
  // Kill the process. This is important for security since most of code
  // does not check the result of memory allocation.
  // https://msdn.microsoft.com/en-us/library/het71c37.aspx
  // Pass the size of the failed request in an exception argument.
  ULONG_PTR exception_args[] = {0};
  ::RaiseException(kOomExceptionCode, EXCEPTION_NONCONTINUABLE,
                   std::size(exception_args), exception_args);

  // Safety check, make sure process exits here.
  _exit(kOomExceptionCode);
}

NOINLINE void __stdcall AtlThrowImpl(HRESULT hr) {
  Alias(&hr);
  if (hr == E_OUTOFMEMORY) {
    TerminateBecauseOutOfMemory();
  }
  IMMEDIATE_CRASH();
}

#endif  // COMPILER_MSVC
