// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/files/memory_mapped_file.h"
#include "base/check_op.h"
#include "base/logging.h"
// #include "base/system/sys_info.h"
#include "base/numerics/safe_math.h"

static size_t VMAllocationGranularity();

#ifdef _WIN32
#include <windows.h>
static const _SYSTEM_INFO& GetSystemInfoStorage() {
  static const _SYSTEM_INFO system_info = [] {
    _SYSTEM_INFO info = {};
    ::GetNativeSystemInfo(&info);
    return info;
  }();
  return system_info;
}
static size_t VMAllocationGranularity() {
  return GetSystemInfoStorage().dwAllocationGranularity;
}
#else
#include <unistd.h>
static size_t VMAllocationGranularity() {
  return getpagesize();
}
#endif

namespace gurl_base {
const MemoryMappedFile::Region MemoryMappedFile::Region::kWholeFile = {0, 0};
MemoryMappedFile::~MemoryMappedFile() {
  CloseHandles();
}
bool MemoryMappedFile::Initialize(PlatformFile file, Access access) {
  GURL_DCHECK_NE(READ_WRITE_EXTEND, access);
  return Initialize(std::move(file), Region::kWholeFile, access);
}
bool MemoryMappedFile::Initialize(PlatformFile file,
                                  const Region& region,
                                  Access access) {
  switch (access) {
    case READ_WRITE_EXTEND:
      GURL_DCHECK(Region::kWholeFile != region);
      {
        CheckedNumeric<int64_t> region_end(region.offset);
        region_end += region.size;
        if (!region_end.IsValid()) {
          GURL_DLOG(ERROR) << "Region bounds exceed maximum for base::File.";
          return false;
        }
      }
      [[fallthrough]];
    case READ_ONLY:
    case READ_WRITE:
    case READ_WRITE_COPY:
      // Ensure that the region values are valid.
      if (region.offset < 0) {
        GURL_DLOG(ERROR) << "Region bounds are not valid.";
        return false;
      }
      break;
#if BUILDFLAG(IS_WIN)
    case READ_CODE_IMAGE:
      // Can't open with "READ_CODE_IMAGE", not supported outside Windows
      // or with a |region|.
      GURL_NOTREACHED();
      break;
#endif
  }
  if (IsValid())
    return false;
  if (region != Region::kWholeFile)
    GURL_DCHECK_GE(region.offset, 0);
  file_ = std::move(file);
  if (!MapFileRegionToMemory(region, access)) {
    CloseHandles();
    return false;
  }
  return true;
}
bool MemoryMappedFile::IsValid() const {
  return data_ != nullptr;
}
// static
void MemoryMappedFile::CalculateVMAlignedBoundaries(int64_t start,
                                                    size_t size,
                                                    int64_t* aligned_start,
                                                    size_t* aligned_size,
                                                    int32_t* offset) {
  // Sadly, on Windows, the mmap alignment is not just equal to the page size.
  uint64_t mask = VMAllocationGranularity() - 1;
  GURL_CHECK(IsValueInRangeForNumericType<int32_t>(mask));
  *offset = static_cast<int32_t>(static_cast<uint64_t>(start) & mask);
  *aligned_start = static_cast<int64_t>(static_cast<uint64_t>(start) & ~mask);
  // The DCHECK above means bit 31 is not set in `mask`, which in turn means
  // *offset is positive.  Therefore casting it to a size_t is safe.
  *aligned_size =
      (size + static_cast<size_t>(*offset) + static_cast<size_t>(mask)) & ~mask;
}
}  // namespace gurl_base
