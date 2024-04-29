// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022-2024 Chilledheart  */

#ifndef CORE_TEMPLATE_UTIL_INTERNAL_H
#define CORE_TEMPLATE_UTIL_INTERNAL_H

#include <stddef.h>
#include <iosfwd>
#include <iterator>
#include <type_traits>
#include <utility>

#include <base/compiler_specific.h>

namespace internal {

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T, decltype(void(std::declval<std::ostream&>() << std::declval<T>()))> : std::true_type {
};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};
template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))> : std::true_type {};

}  // namespace internal

#endif  // CORE_TEMPLATE_UTIL_INTERNAL_H
