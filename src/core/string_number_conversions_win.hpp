// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#ifndef CORE_STRINGS_STRING_NUMBER_CONVERSIONS_WIN_H_
#define CORE_STRINGS_STRING_NUMBER_CONVERSIONS_WIN_H_

#include <string>

std::wstring NumberToWString(int value);
std::wstring NumberToWString(unsigned int value);
std::wstring NumberToWString(long value);
std::wstring NumberToWString(unsigned long value);
std::wstring NumberToWString(long long value);
std::wstring NumberToWString(unsigned long long value);
std::wstring NumberToWString(double value);

// The following section contains overloads of the cross-platform APIs for
// std::wstring and base::const std::wstring&.
bool StringToInt(const std::wstring& input, int* output);
bool StringToUint(const std::wstring& input, unsigned* output);
bool StringToInt64(const std::wstring& input, int64_t* output);
bool StringToUint64(const std::wstring& input, uint64_t* output);
bool StringToSizeT(const std::wstring& input, size_t* output);
bool StringToDouble(const std::wstring& input, double* output);

#endif  // CORE_STRINGS_STRING_NUMBER_CONVERSIONS_WIN_H_
