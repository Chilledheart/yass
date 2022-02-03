// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_FUNCTIONAL_IDENTITY_H_
#define CORE_FUNCTIONAL_IDENTITY_H_

#include <utility>

// Implementation of C++20's std::identity.
//
// Reference:
// - https://en.cppreference.com/w/cpp/utility/functional/identity
// - https://wg21.link/func.identity
struct identity {
  template <typename T>
  constexpr T&& operator()(T&& t) const noexcept {
    return std::forward<T>(t);
  }

  using is_transparent = void;
};

#endif  // CORE_FUNCTIONAL_IDENTITY_H_
