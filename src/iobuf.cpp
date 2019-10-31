//
// iobuf.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "iobuf.hpp"

inline size_t goodMallocSize(size_t minSize) noexcept { return minSize; }

/**
 * Trivial wrappers around malloc, calloc, realloc that check for allocation
 * failure and throw std::bad_alloc in that case.
 */
inline void *checkedMalloc(size_t size) {
  void *p = malloc(size);
  if (!p) {
    throw std::bad_alloc();
  }
  return p;
}

inline void *checkedCalloc(size_t n, size_t size) {
  void *p = calloc(n, size);
  if (!p) {
    throw std::bad_alloc();
  }
  return p;
}

inline void *checkedRealloc(void *ptr, size_t size) {
  void *p = realloc(ptr, size);
  if (!p) {
    throw std::bad_alloc();
  }
  return p;
}

static inline size_t goodExtBufferSize(std::size_t minCapacity) {
  // Determine how much space we should allocate.  We'll store the SharedInfo
  // for the external buffer just after the buffer itself.  (We store it just
  // after the buffer rather than just before so that the code can still just
  // use free(buf_) to free the buffer.)
  size_t minSize = static_cast<size_t>(minCapacity);
  // Add room for padding so that the SharedInfo will be aligned on an 8-byte
  // boundary.
  minSize = (minSize + 7) & ~7;

  // Use goodMallocSize() to bump up the capacity to a decent size to request
  // from malloc, so we can use all of the space that malloc will probably give
  // us anyway.
  return goodMallocSize(minSize);
}

IOBuf::IOBuf(CreateOp, std::size_t capacity) : data_(nullptr), length_(0) {
  capacity_ = goodExtBufferSize(capacity);
  buf_ = static_cast<uint8_t *>(checkedMalloc(capacity));
  data_ = buf_;
}

IOBuf::IOBuf(CopyBufferOp /* op */, const void *buf, std::size_t size,
             std::size_t headroom, std::size_t minTailroom)
    : IOBuf(CREATE, headroom + size + minTailroom) {
  advance(headroom);
  if (size > 0) {
    assert(buf != nullptr);
    memcpy(mutable_data(), buf, size);
    append(size);
  }
}

IOBuf::IOBuf(CopyBufferOp op, ByteRange br, std::size_t headroom,
             std::size_t minTailroom)
    : IOBuf(op, br.data(), br.size(), headroom, minTailroom) {}

IOBuf::~IOBuf() {
  if (buf_)
    free(buf_);
}

std::unique_ptr<IOBuf> IOBuf::create(std::size_t capacity) {
  return std::make_unique<IOBuf>(CREATE, capacity);
}

std::unique_ptr<IOBuf> IOBuf::clone() const {
  return std::make_unique<IOBuf>(cloneAsValue());
}

IOBuf IOBuf::cloneAsValue() const {
  return IOBuf(InternalConstructor(), buf_, capacity_, data_, length_);
}

IOBuf::IOBuf() noexcept {}

IOBuf::IOBuf(IOBuf &&other) noexcept
    : buf_(other.buf_), data_(other.data_), length_(other.length_),
      capacity_(other.capacity_) {
  // Reset other so it is a clean state to be destroyed.
  other.buf_ = nullptr;
  other.data_ = nullptr;
  other.length_ = 0;
  other.capacity_ = 0;
}

IOBuf::IOBuf(const IOBuf &other) { *this = other.cloneAsValue(); }

IOBuf::IOBuf(InternalConstructor, uint8_t *buf, std::size_t capacity,
             uint8_t *data, std::size_t length) noexcept
    : buf_(buf), data_(data), length_(length), capacity_(capacity) {
  assert(data >= buf);
  assert(data + length <= buf + capacity);
}

IOBuf &IOBuf::operator=(IOBuf &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  data_ = other.data_;
  buf_ = other.buf_;
  length_ = other.length_;
  capacity_ = other.capacity_;
  // Reset other so it is a clean state to be destroyed.
  other.buf_ = nullptr;
  other.data_ = nullptr;
  other.length_ = 0;
  other.capacity_ = 0;

  return *this;
}

IOBuf &IOBuf::operator=(const IOBuf &other) {
  if (this != &other) {
    *this = IOBuf(other);
  }
  return *this;
}

void IOBuf::reserveSlow(std::size_t minHeadroom, std::size_t minTailroom) {
  size_t newCapacity = (size_t)length_ + minHeadroom + minTailroom;
  DCHECK_LT(newCapacity, UINT32_MAX);

  // We'll need to reallocate the buffer.
  // There are a few options.
  // - If we have enough total room, move the data around in the buffer
  //   and adjust the data_ pointer.
  // - If we're using an internal buffer, we'll switch to an external
  //   buffer with enough headroom and tailroom.
  // - If we have enough headroom (headroom() >= minHeadroom) but not too much
  //   (so we don't waste memory), we can try one of two things, depending on
  //   whether we use jemalloc or not:
  //   - If using jemalloc, we can try to expand in place, avoiding a memcpy()
  //   - If not using jemalloc and we don't have too much to copy,
  //     we'll use realloc() (note that realloc might have to copy
  //     headroom + data + tailroom, see smartRealloc in folly/memory/Malloc.h)
  // - Otherwise, bite the bullet and reallocate.
  if (headroom() + tailroom() >= minHeadroom + minTailroom) {
    uint8_t *newData = mutable_buffer() + minHeadroom;
    memmove(newData, data_, length_);
    data_ = newData;
    return;
  }

  size_t newAllocatedCapacity = 0;
  uint8_t *newBuffer = nullptr;
  std::size_t newHeadroom = 0;
  std::size_t oldHeadroom = headroom();

  if (length_ && oldHeadroom >= minHeadroom) {
    size_t headSlack = oldHeadroom - minHeadroom;
    newAllocatedCapacity = goodExtBufferSize(newCapacity + headSlack);
    size_t copySlack = capacity() - length_;
    if (copySlack * 2 <= length_) {
      void *p = realloc(buf_, newAllocatedCapacity);
      if (p == nullptr) {
        throw std::bad_alloc();
      }
      newBuffer = static_cast<uint8_t *>(p);
      newHeadroom = oldHeadroom;
    }
  }

  // None of the previous reallocation strategies worked (or we're using
  // an internal buffer).  malloc/copy/free.
  if (newBuffer == nullptr) {
    newAllocatedCapacity = goodExtBufferSize(newCapacity);
    newBuffer = static_cast<uint8_t *>(checkedMalloc(newAllocatedCapacity));
    if (length_ > 0) {
      assert(data_ != nullptr);
      memcpy(newBuffer + minHeadroom, data_, length_);
    }
    newHeadroom = minHeadroom;
    free(buf_);
  }

  // std::size_t cap;
  // initExtBuffer(newBuffer, newAllocatedCapacity, &info, &cap);

  capacity_ = newAllocatedCapacity;
  buf_ = newBuffer;
  data_ = newBuffer + newHeadroom;
}
