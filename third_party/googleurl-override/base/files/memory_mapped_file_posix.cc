// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/files/memory_mapped_file.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
// #include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
// #include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
namespace gurl_base {
MemoryMappedFile::MemoryMappedFile() = default;
#if !BUILDFLAG(IS_NACL)
static int64_t GetLength(PlatformFile file) {
  struct stat file_info {};
  if (fstat(file, &file_info))
    return -1;
  return file_info.st_size;
}
bool MemoryMappedFile::MapFileRegionToMemory(
    const MemoryMappedFile::Region& region,
    Access access) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  off_t map_start = 0;
  size_t map_size = 0;
  int32_t data_offset = 0;
  if (region == MemoryMappedFile::Region::kWholeFile) {
    int64_t file_len = GetLength(file_);
    if (file_len < 0) {
      GURL_DLOG(ERROR) << "fstat " << file_;
      return false;
    }
    if (!IsValueInRangeForNumericType<size_t>(file_len))
      return false;
    map_size = static_cast<size_t>(file_len);
    length_ = map_size;
  } else {
    // The region can be arbitrarily aligned. mmap, instead, requires both the
    // start and size to be page-aligned. Hence, we map here the page-aligned
    // outer region [|aligned_start|, |aligned_start| + |size|] which contains
    // |region| and then add up the |data_offset| displacement.
    int64_t aligned_start = 0;
    size_t aligned_size = 0;
    CalculateVMAlignedBoundaries(region.offset,
                                 region.size,
                                 &aligned_start,
                                 &aligned_size,
                                 &data_offset);
    // Ensure that the casts in the mmap call below are sane.
    if (aligned_start < 0 ||
        !IsValueInRangeForNumericType<off_t>(aligned_start)) {
      GURL_DLOG(ERROR) << "Region bounds are not valid for mmap";
      return false;
    }
    map_start = static_cast<off_t>(aligned_start);
    map_size = aligned_size;
    length_ = region.size;
  }
  int prot = 0;
  int flags = MAP_SHARED;
  switch (access) {
    case READ_ONLY:
      prot |= PROT_READ;
      break;
    // NOT IMPLEMENTED
    default:
      return false;
#if 0
    case READ_WRITE:
      prot |= PROT_READ | PROT_WRITE;
      break;
    case READ_WRITE_COPY:
      prot |= PROT_READ | PROT_WRITE;
      flags = MAP_PRIVATE;
      break;
    case READ_WRITE_EXTEND:
      prot |= PROT_READ | PROT_WRITE;
      if (!AllocateFileRegion(&file_, region.offset, region.size))
        return false;
      break;
#endif
  }
  data_ = static_cast<uint8_t*>(
      mmap(nullptr, map_size, prot, flags, file_, map_start));
  if (data_ == MAP_FAILED) {
    GURL_LOG(ERROR) << "mmap " << file_;
    return false;
  }
  data_ += data_offset;
  return true;
}
#endif
void MemoryMappedFile::CloseHandles() {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  if (data_ != nullptr) {
    munmap(data_, length_);
  }
#ifdef _WIN32
  CloseHandle(file_);
#else
  close(file_);
#endif
  length_ = 0;
}
}  // namespace base
