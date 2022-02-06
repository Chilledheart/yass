// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_RANGES_FUNCTIONAL_H_
#define CORE_RANGES_FUNCTIONAL_H_

#include <functional>
#include <type_traits>
#include <utility>

namespace ranges {

// Simplified implementations of C++20's std::ranges comparison function
// objects. As opposed to the std::ranges implementation, these versions do not
// constrain the passed-in types.
//
// Reference: https://wg21.link/range.cmp
using equal_to = std::equal_to<>;
using not_equal_to = std::not_equal_to<>;
using greater = std::greater<>;
using less = std::less<>;
using greater_equal = std::greater_equal<>;
using less_equal = std::less_equal<>;

}  // namespace ranges

#endif  // CORE_RANGES_FUNCTIONAL_H_
