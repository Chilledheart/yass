// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CORE_ATL
#define H_CORE_ATL

#ifdef COMPILER_MSVC

// Check no prior poisonous defines were made.
#define StrCat StrCat
// Undefine before windows header will make the poisonous defines
#undef StrCat

// atlwin.h relies on std::void_t, but libc++ doesn't define it unless
// _LIBCPP_STD_VER > 14.  Workaround this by manually defining it.
#include <type_traits>
#if defined(_LIBCPP_STD_VER) && _LIBCPP_STD_VER <= 14
namespace std {
template <class...>
using void_t = void;
}
#endif

// Declare our own exception thrower (atl_throw.h includes atldef.h).
#include "atl_throw.hpp"

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlhost.h>
#include <atlsecurity.h>
#include <atlwin.h>

// Undefine the poisonous defines
#undef StrCat
// Check no poisonous defines follow this include
#define StrCat StrCat

#endif  // COMPILER_MSVC

#endif  // H_CORE_ATL
