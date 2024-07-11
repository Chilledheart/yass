// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/rand_util.hpp"

#ifdef _WIN32

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <algorithm>
#include <limits>

#include <build/build_config.h>
#include "core/logging.hpp"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace {

// wine's trick
decltype(&ProcessPrng) GetProcessPrngFallback() {
  HMODULE hmod = LoadLibraryW(L"advapi32");
  CHECK(hmod);
  auto process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(reinterpret_cast<void*>(GetProcAddress(hmod, "SystemFunction036")));
  CHECK(process_prng_fn);
  return process_prng_fn;
}

decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives");
  if (hmod == nullptr) {
    return GetProcessPrngFallback();
  }
  auto process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(reinterpret_cast<void*>(GetProcAddress(hmod, "ProcessPrng")));
  if (process_prng_fn == nullptr) {
    // Possible on Windows 7 SP1, need fallback as well
    return GetProcessPrngFallback();
  }
  return process_prng_fn;
}

}  // namespace

void RandBytes(void* output, size_t output_length) {
  BYTE* output_ptr = static_cast<BYTE*>(output);
  while (output_length > 0) {
    static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
    const ULONG output_bytes_this_pass =
        static_cast<ULONG>(std::min(output_length, static_cast<size_t>(std::numeric_limits<ULONG>::max())));
    const BOOL success = process_prng_fn(output_ptr, output_bytes_this_pass);
    // ProcessPrng is documented to always return TRUE.
    CHECK(success);
    output_length -= output_bytes_this_pass;
    output_ptr += output_bytes_this_pass;
  }
}

#endif  // _WIN32
