// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/string_number_conversions_win.hpp"

#include <string>

#include "core/string_number_conversions_internal.hpp"

std::wstring NumberToWString(int value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(long long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned long long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(double value) {
  return internal::DoubleToStringT<std::wstring>(value);
}

namespace internal {

template <>
class internal::WhitespaceHelper<wchar_t> {
 public:
  static bool Invoke(wchar_t c) { return 0 != iswspace(c); }
};

}  // namespace internal

bool StringToInt(const std::wstring& input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(const std::wstring& input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(const std::wstring& input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(const std::wstring& input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(const std::wstring& input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToDouble(const std::wstring& input, double* output) {
  return internal::StringToDoubleImpl(input, input.data(), *output);
}
