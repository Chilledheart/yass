// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/files/memory_mapped_file.h"
#include <stddef.h>
#include <stdint.h>
#include <limits>
#include <string>
// #include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
// #include "base/threading/scoped_blocking_call.h"
#include "base/win/pe_image.h"
#include <windows.h>
#include <winnt.h>  // NOLINT(build/include_order)
#ifndef SEC_IMAGE_NO_EXECUTE
// This value is not supported before Windows Server 2012 and Windows 8
// #define SEC_IMAGE_NO_EXECUTE (SEC_IMAGE | SEC_NOCACHE)
#define SEC_IMAGE_NO_EXECUTE SEC_NOCACHE
#endif
static int64_t GetLength(gurl_base::PlatformFile file) {
  LARGE_INTEGER size;
  if (!::GetFileSizeEx(file, &size))
    return -1;
  return static_cast<int64_t>(size.QuadPart);
}
namespace gurl_base {
MemoryMappedFile::MemoryMappedFile() = default;
bool MemoryMappedFile::MapImageToMemory(Access access) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  // The arguments to the calls of ::CreateFile(), ::CreateFileMapping(), and
  // ::MapViewOfFile() need to be self consistent as far as access rights and
  // type of mapping or one or more of them will fail in non-obvious ways.
  if (file_ == kInvalidPlatformFile)
    return false;
  file_mapping_.Set(::CreateFileMapping(file_, nullptr,
                                        PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0,
                                        0, NULL));
  if (!file_mapping_.is_valid())
    return false;
  data_ = static_cast<uint8_t*>(
      ::MapViewOfFile(file_mapping_.get(), FILE_MAP_READ, 0, 0, 0));
  if (!data_)
    return false;
  // We need to know how large the mapped file is in some cases
  gurl_base::win::PEImage pe_image(data_);
  length_ = pe_image.GetNTHeaders()->OptionalHeader.SizeOfImage;
  return true;
}
bool MemoryMappedFile::MapFileRegionToMemory(
    const MemoryMappedFile::Region& region,
    Access access) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  GURL_DCHECK(access != READ_CODE_IMAGE || region == Region::kWholeFile);
  if (file_ == kInvalidPlatformFile)
    return false;
  DWORD view_access;
  DWORD flags = 0;
  ULARGE_INTEGER size = {};
  switch (access) {
    case READ_ONLY:
      flags |= PAGE_READONLY;
      view_access = FILE_MAP_READ;
      break;
    case READ_WRITE:
      flags |= PAGE_READWRITE;
      view_access = FILE_MAP_WRITE;
      break;
    case READ_WRITE_COPY:
      flags |= PAGE_WRITECOPY;
      view_access = FILE_MAP_COPY;
      break;
    case READ_WRITE_EXTEND:
      flags |= PAGE_READWRITE;
      view_access = FILE_MAP_WRITE;
      size.QuadPart = region.size;
      break;
    case READ_CODE_IMAGE:
      return MapImageToMemory(access);
  }
  file_mapping_.Set(::CreateFileMapping(file_, NULL, flags,
                                        size.HighPart, size.LowPart, NULL));
  if (!file_mapping_.is_valid())
    return false;
  ULARGE_INTEGER map_start = {};
  SIZE_T map_size = 0;
  int32_t data_offset = 0;
  if (region == MemoryMappedFile::Region::kWholeFile) {
    GURL_DCHECK_NE(READ_WRITE_EXTEND, access);
    int64_t file_len = GetLength(file_);
    if (file_len <= 0 || !IsValueInRangeForNumericType<size_t>(file_len))
      return false;
    length_ = static_cast<size_t>(file_len);
  } else {
    // The region can be arbitrarily aligned. MapViewOfFile, instead, requires
    // that the start address is aligned to the VM granularity (which is
    // typically larger than a page size, for instance 32k).
    // Also, conversely to POSIX's mmap, the |map_size| doesn't have to be
    // aligned and must be less than or equal the mapped file size.
    // We map here the outer region [|aligned_start|, |aligned_start+size|]
    // which contains |region| and then add up the |data_offset| displacement.
    int64_t aligned_start = 0;
    size_t ignored = 0U;
    CalculateVMAlignedBoundaries(region.offset, region.size, &aligned_start,
                                 &ignored, &data_offset);
    gurl_base::CheckedNumeric<SIZE_T> full_map_size = region.size;
    full_map_size += data_offset;
    // Ensure that the casts below in the MapViewOfFile call are sane.
    if (aligned_start < 0 || !full_map_size.IsValid()) {
      GURL_DLOG(ERROR) << "Region bounds are not valid for MapViewOfFile";
      return false;
    }
    map_start.QuadPart = static_cast<uint64_t>(aligned_start);
    map_size = full_map_size.ValueOrDie();
    length_ = region.size;
  }
  data_ = static_cast<uint8_t*>(::MapViewOfFile(file_mapping_.get(),
                                                view_access, map_start.HighPart,
                                                map_start.LowPart, map_size));
  if (data_ == nullptr)
    return false;
  data_ += data_offset;
  return true;
}
void MemoryMappedFile::CloseHandles() {
  if (data_)
    ::UnmapViewOfFile(data_);
  if (file_mapping_.is_valid())
    file_mapping_.Close();
  if (file_ != kInvalidPlatformFile)
    CloseHandle(file_);
  data_ = nullptr;
  length_ = 0;
}
}  // namespace gurl_base
