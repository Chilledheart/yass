// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_FUNCTIONAL_NOT_FN_H_
#define CORE_FUNCTIONAL_NOT_FN_H_

#include <type_traits>
#include <utility>

#include "core/functional/invoke.hpp"

namespace internal {

template <typename F>
struct NotFnImpl {
  F f;

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) & noexcept {
    return !base::invoke(f, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) const& noexcept {
    return !base::invoke(f, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) && noexcept {
    return !base::invoke(std::move(f), std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr decltype(auto) operator()(Args&&... args) const&& noexcept {
    return !base::invoke(std::move(f), std::forward<Args>(args)...);
  }
};

}  // namespace internal

// Implementation of C++17's std::not_fn.
//
// Reference:
// - https://en.cppreference.com/w/cpp/utility/functional/not_fn
// - https://wg21.link/func.not.fn
template <typename F>
constexpr internal::NotFnImpl<std::decay_t<F>> not_fn(F&& f) {
  return {std::forward<F>(f)};
}

#endif  // CORE_FUNCTIONAL_NOT_FN_H_
