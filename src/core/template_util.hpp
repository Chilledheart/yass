// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_TEMPLATE_UTIL_INTERNAL_H
#define CORE_TEMPLATE_UTIL_INTERNAL_H

#include <stddef.h>
#include <iosfwd>
#include <iterator>
#include <type_traits>
#include <utility>

#include <base/compiler_specific.h>

template <class T>
struct is_non_const_reference : std::false_type {};
template <class T>
struct is_non_const_reference<T&> : std::true_type {};
template <class T>
struct is_non_const_reference<const T&> : std::false_type {};

namespace internal {

// Implementation detail of void_t below.
template <typename...>
struct make_void {
  using type = void;
};

}  // namespace internal

// :void_t is an implementation of std::void_t from C++17.
//
// We use |internal::make_void| as a helper struct to avoid a C++14
// defect:
//   http://en.cppreference.com/w/cpp/types/void_t
//   http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#1558
template <typename... Ts>
using void_t = typename ::internal::make_void<Ts...>::type;

namespace internal {

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};
template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))>
    : std::true_type {};

// Used to detech whether the given type is an iterator.  This is normally used
// with std::enable_if to provide disambiguation for functions that take
// templatzed iterators as input.
template <typename T, typename = void>
struct is_iterator : std::false_type {};

template <typename T>
struct is_iterator<T,
                   void_t<typename std::iterator_traits<T>::iterator_category>>
    : std::true_type {};

// Helper to express preferences in an overload set. If more than one overload
// are available for a given set of parameters the overload with the higher
// priority will be chosen.
template <size_t I>
struct priority_tag : priority_tag<I - 1> {};

template <>
struct priority_tag<0> {};

}  // namespace internal

// in_place_t is an implementation of std::in_place_t from
// C++17. A tag type used to request in-place construction in template vararg
// constructors.

// Specification:
// https://en.cppreference.com/w/cpp/utility/in_place
struct in_place_t {};
constexpr in_place_t in_place = {};

// in_place_type_t is an implementation of std::in_place_type_t from
// C++17. A tag type used for in-place construction when the type to construct
// needs to be specified, such as with unique_any, designed to be a
// drop-in replacement.

// Specification:
// http://en.cppreference.com/w/cpp/utility/in_place
template <typename T>
struct in_place_type_t {};

template <typename T>
struct is_in_place_type_t {
  static constexpr bool value = false;
};

template <typename... Ts>
struct is_in_place_type_t<in_place_type_t<Ts...>> {
  static constexpr bool value = true;
};

// C++14 implementation of C++17's std::bool_constant.
//
// Reference: https://en.cppreference.com/w/cpp/types/integral_constant
// Specification: https://wg21.link/meta.type.synop
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

// C++14 implementation of C++17's std::conjunction.
//
// Reference: https://en.cppreference.com/w/cpp/types/conjunction
// Specification: https://wg21.link/meta.logical#1.itemdecl:1
template <typename...>
struct conjunction : std::true_type {};

template <typename B1>
struct conjunction<B1> : B1 {};

template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : std::conditional_t<static_cast<bool>(B1::value), conjunction<Bn...>, B1> {
};

// C++14 implementation of C++17's std::disjunction.
//
// Reference: https://en.cppreference.com/w/cpp/types/disjunction
// Specification: https://wg21.link/meta.logical#itemdecl:2
template <typename...>
struct disjunction : std::false_type {};

template <typename B1>
struct disjunction<B1> : B1 {};

template <typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : std::conditional_t<static_cast<bool>(B1::value), B1, disjunction<Bn...>> {
};

// C++14 implementation of C++17's std::negation.
//
// Reference: https://en.cppreference.com/w/cpp/types/negation
// Specification: https://wg21.link/meta.logical#itemdecl:3
template <typename B>
struct negation : bool_constant<!static_cast<bool>(B::value)> {};

// Implementation of C++17's invoke_result.
//
// This implementation adds references to `Functor` and `Args` to work around
// some quirks of std::result_of. See the #Notes section of [1] for details.
//
// References:
// [1] https://en.cppreference.com/w/cpp/types/result_of
// [2] https://wg21.link/meta.trans.other#lib:invoke_result
template <typename Functor, typename... Args>
// result_of is deprecated in c++17, so don't use it there.
using invoke_result = std::invoke_result<Functor, Args...>;

// Implementation of C++17's std::invoke_result_t.
//
// Reference: https://wg21.link/meta.type.synop#lib:invoke_result_t
template <typename Functor, typename... Args>
using invoke_result_t = typename invoke_result<Functor, Args...>::type;

namespace internal {

// Base case, `InvokeResult` does not have a nested type member. This means `F`
// could not be invoked with `Args...` and thus is not invocable.
template <typename InvokeResult, typename R, typename = void>
struct IsInvocableImpl : std::false_type {};

// Happy case, `InvokeResult` does have a nested type member. Now check whether
// `InvokeResult::type` is convertible to `R`. Short circuit in case
// `std::is_void<R>`.
template <typename InvokeResult, typename R>
struct IsInvocableImpl<InvokeResult, R, void_t<typename InvokeResult::type>>
    : disjunction<std::is_void<R>,
                  std::is_convertible<typename InvokeResult::type, R>> {};

}  // namespace internal

// Implementation of C++17's std::is_invocable_r.
//
// Returns whether `F` can be invoked with `Args...` and the result is
// convertible to `R`.
//
// Reference: https://wg21.link/meta.rel#lib:is_invocable_r
template <typename R, typename F, typename... Args>
struct is_invocable_r
    : internal::IsInvocableImpl<invoke_result<F, Args...>, R> {};

// Implementation of C++17's std::is_invocable.
//
// Returns whether `F` can be invoked with `Args...`.
//
// Reference: https://wg21.link/meta.rel#lib:is_invocable
template <typename F, typename... Args>
struct is_invocable : is_invocable_r<void, F, Args...> {};

namespace internal {

// The indirection with std::is_enum<T> is required, because instantiating
// std::underlying_type_t<T> when T is not an enum is UB prior to C++20.
template <typename T, bool = std::is_enum<T>::value>
struct IsScopedEnumImpl : std::false_type {};

template <typename T>
struct IsScopedEnumImpl<T, /*std::is_enum<T>::value=*/true>
    : negation<std::is_convertible<T, std::underlying_type_t<T>>> {};

}  // namespace internal

// Implementation of C++23's std::is_scoped_enum
//
// Reference: https://en.cppreference.com/w/cpp/types/is_scoped_enum
template <typename T>
struct is_scoped_enum : internal::IsScopedEnumImpl<T> {};

// Implementation of C++20's std::remove_cvref.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.trans.other#lib:remove_cvref
template <typename T>
struct remove_cvref {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

// Implementation of C++20's std::remove_cvref_t.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.type.synop#lib:remove_cvref_t
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Implementation of C++20's std::is_constant_evaluated.
//
// References:
// - https://en.cppreference.com/w/cpp/types/is_constant_evaluated
// - https://wg21.link/meta.const.eval
constexpr bool is_constant_evaluated() noexcept {
#if HAS_BUILTIN(__builtin_is_constant_evaluated)
  return __builtin_is_constant_evaluated();
#else
  return false;
#endif
}

// Simplified implementation of C++20's std::iter_value_t.
// As opposed to std::iter_value_t, this implementation does not restrict
// the type of `Iter` and does not consider specializations of
// `indirectly_readable_traits`.
//
// Reference: https://wg21.link/readable.traits#2
template <typename Iter>
using iter_value_t =
    typename std::iterator_traits<remove_cvref_t<Iter>>::value_type;

// Simplified implementation of C++20's std::iter_reference_t.
// As opposed to std::iter_reference_t, this implementation does not restrict
// the type of `Iter`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=iter_reference_t
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Simplified implementation of C++20's std::indirect_result_t. As opposed to
// std::indirect_result_t, this implementation does not restrict the type of
// `Func` and `Iters`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=indirect_result_t
template <typename Func, typename... Iters>
using indirect_result_t = invoke_result_t<Func, iter_reference_t<Iters>...>;

// Simplified implementation of C++20's std::projected. As opposed to
// std::projected, this implementation does not explicitly restrict the type of
// `Iter` and `Proj`, but rather does so implicitly by requiring
// `indirect_result_t<Proj, Iter>` is a valid type. This is required for SFINAE
// friendliness.
//
// Reference: https://wg21.link/projected
template <typename Iter,
          typename Proj,
          typename IndirectResultT = indirect_result_t<Proj, Iter>>
struct projected {
  using value_type = remove_cvref_t<IndirectResultT>;

  IndirectResultT operator*() const;  // not defined
};

#endif  // CORE_TEMPLATE_UTIL_INTERNAL_H
