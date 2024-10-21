// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <limits>

#include "base/check.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace gurl_base {

namespace {

decltype(&ProcessPrng) GetProcessPrngFallback() {
  HMODULE hmod = LoadLibraryW(L"advapi32");
  GURL_CHECK(hmod);
  auto process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          reinterpret_cast<void*>(GetProcAddress(hmod, "SystemFunction036")));
  GURL_CHECK(process_prng_fn);
  return process_prng_fn;
}

// Import bcryptprimitives!ProcessPrng rather than cryptbase!RtlGenRandom to
// avoid opening a handle to \\Device\KsecDD in the renderer.
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  if (hmod == nullptr) {
    return GetProcessPrngFallback();
  }
  auto process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          reinterpret_cast<void*>(GetProcAddress(hmod, "ProcessPrng")));
  if (process_prng_fn == nullptr) {
    // Possible on Windows 7 SP1, need fallback as well
    return GetProcessPrngFallback();
  }
  return process_prng_fn;
}

void RandBytes(void* output, size_t output_length, bool avoid_allocation) {
  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  BOOL success = process_prng_fn(static_cast<BYTE*>(output), output_length);
  // ProcessPrng is documented to always return TRUE.
  GURL_CHECK(success);
}

}  // namespace

void RandBytes(void* output, size_t output_length) {
  RandBytes(output, output_length, /*avoid_allocation=*/false);
}

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  RandBytes(&number, sizeof(number), /*avoid_allocation=*/true);
  // This transformation is explained in rand_util.cc.
  return (number >> 11) * 0x1.0p-53;
}

}  // namespace internal

}  // namespace gurl_base
