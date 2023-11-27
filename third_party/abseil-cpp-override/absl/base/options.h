#ifndef ABSL_BASE_OPTIONS_H_
#define ABSL_BASE_OPTIONS_H_
// Include a standard library header to allow configuration based on the
// standard library in use.
#ifdef __cplusplus
#include <ciso646>
#endif
// -----------------------------------------------------------------------------
// Type Compatibility Options
// -----------------------------------------------------------------------------
//
// ABSL_OPTION_USE_STD_ANY
//
// This option controls whether absl::any is implemented as an alias to
// std::any, or as an independent implementation.
//
// A value of 0 means to use Abseil's implementation.  This requires only C++11
// support, and is expected to work on every toolchain we support.
//
// A value of 1 means to use an alias to std::any.  This requires that all code
// using Abseil is built in C++17 mode or later.
//
// A value of 2 means to detect the C++ version being used to compile Abseil,
// and use an alias only if a working std::any is available.  This option is
// useful when you are building your entire program, including all of its
// dependencies, from source.  It should not be used otherwise -- for example,
// if you are distributing Abseil in a binary package manager -- since in
// mode 2, absl::any will name a different type, with a different mangled name
// and binary layout, depending on the compiler flags passed by the end user.
// For more info, see https://abseil.io/about/design/dropin-types.
//
// User code should not inspect this macro.  To check in the preprocessor if
// absl::any is a typedef of std::any, use the feature macro ABSL_USES_STD_ANY.
#define ABSL_OPTION_USE_STD_ANY 2
// ABSL_OPTION_USE_STD_OPTIONAL
//
// This option controls whether absl::optional is implemented as an alias to
// std::optional, or as an independent implementation.
//
// A value of 0 means to use Abseil's implementation.  This requires only C++11
// support, and is expected to work on every toolchain we support.
//
// A value of 1 means to use an alias to std::optional.  This requires that all
// code using Abseil is built in C++17 mode or later.
//
// A value of 2 means to detect the C++ version being used to compile Abseil,
// and use an alias only if a working std::optional is available.  This option
// is useful when you are building your program from source.  It should not be
// used otherwise -- for example, if you are distributing Abseil in a binary
// package manager -- since in mode 2, absl::optional will name a different
// type, with a different mangled name and binary layout, depending on the
// compiler flags passed by the end user.  For more info, see
// https://abseil.io/about/design/dropin-types.
// User code should not inspect this macro.  To check in the preprocessor if
// absl::optional is a typedef of std::optional, use the feature macro
// ABSL_USES_STD_OPTIONAL.
#define ABSL_OPTION_USE_STD_OPTIONAL 2
// ABSL_OPTION_USE_STD_STRING_VIEW
//
// This option controls whether absl::string_view is implemented as an alias to
// std::string_view, or as an independent implementation.
//
// A value of 0 means to use Abseil's implementation.  This requires only C++11
// support, and is expected to work on every toolchain we support.
//
// A value of 1 means to use an alias to std::string_view.  This requires that
// all code using Abseil is built in C++17 mode or later.
//
// A value of 2 means to detect the C++ version being used to compile Abseil,
// and use an alias only if a working std::string_view is available.  This
// option is useful when you are building your program from source.  It should
// not be used otherwise -- for example, if you are distributing Abseil in a
// binary package manager -- since in mode 2, absl::string_view will name a
// different type, with a different mangled name and binary layout, depending on
// the compiler flags passed by the end user.  For more info, see
// https://abseil.io/about/design/dropin-types.
//
// User code should not inspect this macro.  To check in the preprocessor if
// absl::string_view is a typedef of std::string_view, use the feature macro
// ABSL_USES_STD_STRING_VIEW.
#define ABSL_OPTION_USE_STD_STRING_VIEW 2
// ABSL_OPTION_USE_STD_VARIANT
//
// This option controls whether absl::variant is implemented as an alias to
// std::variant, or as an independent implementation.
//
// A value of 0 means to use Abseil's implementation.  This requires only C++11
// support, and is expected to work on every toolchain we support.
//
// A value of 1 means to use an alias to std::variant.  This requires that all
// code using Abseil is built in C++17 mode or later.
//
// A value of 2 means to detect the C++ version being used to compile Abseil,
// and use an alias only if a working std::variant is available.  This option
// is useful when you are building your program from source.  It should not be
// used otherwise -- for example, if you are distributing Abseil in a binary
// package manager -- since in mode 2, absl::variant will name a different
// type, with a different mangled name and binary layout, depending on the
// compiler flags passed by the end user.  For more info, see
// https://abseil.io/about/design/dropin-types.
//
// User code should not inspect this macro.  To check in the preprocessor if
// absl::variant is a typedef of std::variant, use the feature macro
// ABSL_USES_STD_VARIANT.
#define ABSL_OPTION_USE_STD_VARIANT 0
// ABSL_OPTION_USE_INLINE_NAMESPACE
// ABSL_OPTION_INLINE_NAMESPACE_NAME
//
// These options controls whether all entities in the absl namespace are
// contained within an inner inline namespace.  This does not affect the
// user-visible API of Abseil, but it changes the mangled names of all symbols.
//
// This can be useful as a version tag if you are distributing Abseil in
// precompiled form.  This will prevent a binary library build of Abseil with
// one inline namespace being used with headers configured with a different
// inline namespace name.  Binary packagers are reminded that Abseil does not
// guarantee any ABI stability in Abseil, so any update of Abseil or
// configuration change in such a binary package should be combined with a
// new, unique value for the inline namespace name.
//
// A value of 0 means not to use inline namespaces.
//
// A value of 1 means to use an inline namespace with the given name inside
// namespace absl.  If this is set, ABSL_OPTION_INLINE_NAMESPACE_NAME must also
// be changed to a new, unique identifier name.  In particular "head" is not
// allowed.
#define ABSL_OPTION_USE_INLINE_NAMESPACE 0
#define ABSL_OPTION_INLINE_NAMESPACE_NAME head
// ABSL_OPTION_HARDENED
//
// This option enables a "hardened" build in release mode (in this context,
// release mode is defined as a build where the `NDEBUG` macro is defined).
//
// A value of 0 means that "hardened" mode is not enabled.
//
// A value of 1 means that "hardened" mode is enabled.
//
// Hardened builds have additional security checks enabled when `NDEBUG` is
// defined. Defining `NDEBUG` is normally used to turn `assert()` macro into a
// no-op, as well as disabling other bespoke program consistency checks. By
// defining ABSL_OPTION_HARDENED to 1, a select set of checks remain enabled in
// release mode. These checks guard against programming errors that may lead to
// security vulnerabilities. In release mode, when one of these programming
// errors is encountered, the program will immediately abort, possibly without
// any attempt at logging.
//
// The checks enabled by this option are not free; they do incur runtime cost.
//
// The checks enabled by this option are always active when `NDEBUG` is not
// defined, even in the case when ABSL_OPTION_HARDENED is defined to 0. The
// checks enabled by this option may abort the program in a different way and
// log additional information when `NDEBUG` is not defined.
#define ABSL_OPTION_HARDENED 1
#endif  // ABSL_BASE_OPTIONS_H_
