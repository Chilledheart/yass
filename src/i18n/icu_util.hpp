// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */
#ifndef H_I18N_ICU_UTIL_H
#define H_I18N_ICU_UTIL_H

#ifdef HAVE_ICU

#include "base/files/memory_mapped_file.h"
#include "base/files/platform_file.h"

using gurl_base::kInvalidPlatformFile;
using gurl_base::MemoryMappedFile;
using gurl_base::PlatformFile;

#define ICU_UTIL_DATA_FILE 0
#define ICU_UTIL_DATA_STATIC 1

// Call this function to load ICU's data tables for the current process.  This
// function should be called before ICU is used.
bool InitializeICU();

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

// we simply memory map the ICU data file.
// using IcuDataFile = MemoryMappedFile;

// Returns the PlatformFile and Region that was initialized by InitializeICU().
// Use with InitializeICUWithFileDescriptor().
PlatformFile GetIcuDataFileHandle(MemoryMappedFile::Region* out_region);

// Loads ICU data file from file descriptor passed by browser process to
// initialize ICU in render processes.
bool InitializeICUWithFileDescriptor(PlatformFile data_fd, const MemoryMappedFile::Region& data_region);

void ResetGlobalsForTesting();

#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

// In a test binary, initialize functions might be called twice.
void AllowMultipleInitializeCallsForTesting();

#endif  // HAVE_ICU

#endif  // H_I18N_ICU_UTIL_H
