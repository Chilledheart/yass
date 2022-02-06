// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/string_number_conversions.hpp"

#include <iterator>
#include <string>

#include "core/logging.hpp"
#include "core/string_number_conversions_internal.hpp"

#include <absl/strings/string_view.h>

std::string NumberToString(int value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(int value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(long long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(long long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned long long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned long long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(double value) {
  return internal::DoubleToStringT<std::string>(value);
}

#if 0
std::u16string NumberToString16(double value) {
  return internal::DoubleToStringT<std::u16string>(value);
}
#endif

bool StringToInt(absl::string_view input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(absl::string_view input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(absl::string_view input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(absl::string_view input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(absl::string_view input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToDouble(absl::string_view input, double* output) {
  return internal::StringToDoubleImpl(input, input.data(), *output);
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

bool HexStringToInt(absl::string_view input, int* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt(absl::string_view input, uint32_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToInt64(absl::string_view input, int64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt64(absl::string_view input, uint64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToBytes(absl::string_view input, std::vector<uint8_t>* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer(input, std::back_inserter(*output));
}

bool HexStringToString(absl::string_view input, std::string* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer(input, std::back_inserter(*output));
}
