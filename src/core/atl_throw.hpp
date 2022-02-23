// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CORE_ATL_THROW
#define H_CORE_ATL_THROW

#include "core/debug.hpp"
#include "core/immediate_crash.hpp"
#include "core/logging.hpp"

typedef _Return_type_success_(return >= 0) long HRESULT;

#ifdef __ATLDEF_H__
#error atl_throw.h must be included before atldef.h.
#endif

// Defining _ATL_NO_EXCEPTIONS causes ATL to raise a structured exception
// instead of throwing a CAtlException. While crashpad will eventually handle
// this, the HRESULT that caused the problem is lost. So, in addition, define
// our own custom AtlThrow function (_ATL_CUSTOM_THROW).
#ifndef _ATL_NO_EXCEPTIONS
#define _ATL_NO_EXCEPTIONS
#endif

#define _ATL_CUSTOM_THROW
#define AtlThrow ::AtlThrowImpl

// Crash the process forthwith in case of ATL errors.
[[noreturn]] void __stdcall AtlThrowImpl(HRESULT hr);

#include <atldef.h>

// atldef.h mistakenly leaves out the declaration of this function when
// _ATL_CUSTOM_THROW is defined.
namespace ATL {
ATL_NOINLINE __declspec(noreturn) inline void WINAPI AtlThrowLastWin32();
}

#endif  // H_CORE_ATL_THROW
