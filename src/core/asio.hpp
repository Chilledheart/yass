// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef H_CORE_ASIO
#define H_CORE_ASIO

#ifdef _WIN32
#include <malloc.h>  // for _alloca
#endif  // _WIN32

#ifdef _MSC_VER
#pragma push
// #pragma warning(pop): likely mismatch, popping warning state pushed in
// different file
#pragma warning(disable : 5031)
// pointer to potentially throwing function passed to extern C function
#pragma warning(disable : 5039)
#endif  // _MSC_VER

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

#include <asio.hpp>
#include "core/asio_throw_exceptions.hpp"

#pragma GCC diagnostic pop

#ifdef _MSC_VER
#pragma pop
#endif  // _MSC_VER

#endif  // H_CORE_ASIO
