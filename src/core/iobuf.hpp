//
// iobuf.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2019 James Hu (hukeyue at hotmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cinttypes>
#include <cstddef>
#include <stdint.h>
#include <memory>

#include "core/logging.hpp"

#ifndef H_CORE_IOBUF
#define H_CORE_IOBUF

class ByteRange {
public:
  ByteRange(const uint8_t *data, size_t size) : data_(data), size_(size) {}
  ByteRange(const uint8_t *start, const uint8_t *end)
      : data_(start), size_(end - start) {}
  const uint8_t *data() { return data_; }
  size_t size() const { return size_; }

private:
  const uint8_t *data_;
  size_t size_;
};

// [Headroom] - [Data] - [Tailroom]
class IOBuf {
public:
  class Iterator;

  enum CreateOp { CREATE };
  enum CopyBufferOp { COPY_BUFFER };

  typedef ByteRange value_type;

  /**
   * Allocate a new IOBuf object with the requested capacity.
   *
   * Returns a new IOBuf object that must be (eventually) deleted by the
   * caller.  The returned IOBuf may actually have slightly more capacity than
   * requested.
   *
   * The data pointer will initially point to the start of the newly allocated
   * buffer, and will have a data length of 0.
   *
   * Throws std::bad_alloc on error.
   */
  static std::unique_ptr<IOBuf> create(std::size_t capacity);
  IOBuf(CreateOp, std::size_t capacity);

  /**
   * Convenience function to create a new IOBuf object that copies data from a
   * user-supplied buffer, optionally allocating a given amount of
   * headroom and tailroom.
   */
  static std::unique_ptr<IOBuf> copyBuffer(const void *buf, std::size_t size,
                                           std::size_t headroom = 0,
                                           std::size_t minTailroom = 0);
  static std::unique_ptr<IOBuf> copyBuffer(ByteRange br,
                                           std::size_t headroom = 0,
                                           std::size_t minTailroom = 0) {
    return copyBuffer(br.data(), br.size(), headroom, minTailroom);
  }
  IOBuf(CopyBufferOp op, const void *buf, std::size_t size,
        std::size_t headroom = 0, std::size_t minTailroom = 0);
  IOBuf(CopyBufferOp op, ByteRange br, std::size_t headroom = 0,
        std::size_t minTailroom = 0);

  /**
   * Convenience function to create a new IOBuf object that copies data from a
   * user-supplied string, optionally allocating a given amount of
   * headroom and tailroom.
   *
   * Beware when attempting to invoke this function with a constant string
   * literal and a headroom argument: you will likely end up invoking the
   * version of copyBuffer() above.  IOBuf::copyBuffer("hello", 3) will treat
   * the first argument as a const void*, and will invoke the version of
   * copyBuffer() above, with the size argument of 3.
   */
  static std::unique_ptr<IOBuf> copyBuffer(const std::string &buf,
                                           std::size_t headroom = 0,
                                           std::size_t minTailroom = 0);
  IOBuf(CopyBufferOp op, const std::string &buf, std::size_t headroom = 0,
        std::size_t minTailroom = 0)
      : IOBuf(op, buf.data(), buf.size(), headroom, minTailroom) {}

  ~IOBuf();

  // ref to internal buffer/headroom
  const uint8_t *buffer() const { return buf_; }
  uint8_t *mutable_buffer() { return buf_; }

  size_t capacity() const { return capacity_; }

  size_t length() const { return length_; }

  /**
   * Iteration support: a chain of IOBufs may be iterated through using
   * STL-style iterators over const ByteRanges.  Iterators are only invalidated
   * if the IOBuf that they currently point to is removed.
   */
  Iterator cbegin() const;
  Iterator cend() const;
  Iterator begin() const;
  Iterator end() const;

  /**
   * Allocate a new null buffer.
   *
   * This can be used to allocate an empty IOBuf on the stack.  It will have no
   * space allocated for it.  This is generally useful only to later use move
   * assignment to fill out the IOBuf.
   */
  IOBuf() noexcept;

  /**
   * Move constructor and assignment operator.
   *
   * In general, you should only ever move the head of an IOBuf chain.
   * Internal nodes in an IOBuf chain are owned by the head of the chain, and
   * should not be moved from.  (Technically, nothing prevents you from moving
   * a non-head node, but the moved-to node will replace the moved-from node in
   * the chain.  This has implications for ownership, since non-head nodes are
   * owned by the chain head.  You are then responsible for relinquishing
   * ownership of the moved-to node, and manually deleting the moved-from
   * node.)
   *
   * With the move assignment operator, the destination of the move should be
   * the head of an IOBuf chain or a solitary IOBuf not part of a chain.  If
   * the move destination is part of a chain, all other IOBufs in the chain
   * will be deleted.
   */
  IOBuf(IOBuf &&other) noexcept;
  IOBuf &operator=(IOBuf &&other) noexcept;

  IOBuf(const IOBuf &other);
  IOBuf &operator=(const IOBuf &other);

  void reserveSlow(std::size_t minHeadroom, std::size_t minTailroom);

  // ref to data
  const uint8_t *data() const { return data_; }
  uint8_t *mutable_data() { return data_; }

  bool empty() const { return !length(); }

  // ref to tailroom
  const uint8_t *tail() const { return data_ + length_; }
  uint8_t *mutable_tail() { return data_ + length_; }

  size_t headroom() const { return size_t(data_ - buf_); }
  size_t tailroom() const {
    return size_t(buf_ + capacity_ - (data_ + length_));
  }

  /**
   * Shift the data forwards in the buffer.
   *
   * This shifts the data pointer forwards in the buffer to increase the
   * headroom.  This is commonly used to increase the headroom in a newly
   * allocated buffer.
   *
   * The caller is responsible for ensuring that there is sufficient
   * tailroom in the buffer before calling advance().
   *
   * If there is a non-zero data length, advance() will use memmove() to shift
   * the data forwards in the buffer.  In this case, the caller is responsible
   * for making sure the buffer is unshared, so it will not affect other IOBufs
   * that may be sharing the same underlying buffer.
   */
  void advance(size_t amount) {
    DCHECK_LE(amount, tailroom());
    if (amount) {
      memmove(data_ + amount, data_, length_);
    }
    data_ += amount;
  }

  /**
   * Shift the data backwards in the buffer.
   *
   * The caller is responsible for ensuring that there is sufficient headroom
   * in the buffer before calling retreat().
   *
   * If there is a non-zero data length, retreat() will use memmove() to shift
   * the data backwards in the buffer.  In this case, the caller is responsible
   * for making sure the buffer is unshared, so it will not affect other IOBufs
   * that may be sharing the same underlying buffer.
   */
  void retreat(std::size_t amount) {
    // In debug builds, assert if there is a problem.
    DCHECK_LE(amount, headroom());

    if (length_ > 0) {
      memmove(data_ - amount, data_, length_);
    }
    data_ -= amount;
  }

  /**
   * Adjust the data pointer to include more valid data at the beginning.
   *
   * This moves the data pointer backwards to include more of the available
   * buffer.  The caller is responsible for ensuring that there is sufficient
   * headroom for the new data.  The caller is also responsible for populating
   * this section with valid data.
   *
   * This does not modify any actual data in the buffer.
   */
  void prepend(size_t amount) {
    DCHECK_LE(amount, headroom());
    data_ -= amount;
    length_ += amount;
  }

  /**
   * Adjust the tail pointer to include more valid data at the end.
   *
   * This moves the tail pointer forwards to include more of the available
   * buffer.  The caller is responsible for ensuring that there is sufficient
   * tailroom for the new data.  The caller is also responsible for populating
   * this section with valid data.
   *
   * This does not modify any actual data in the buffer.
   */
  void append(std::size_t amount) {
    DCHECK_LE(amount, tailroom());
    length_ += amount;
  }

  /**
   * Adjust the data pointer forwards to include less valid data.
   *
   * This moves the data pointer forwards so that the first amount bytes are no
   * longer considered valid data.  The caller is responsible for ensuring that
   * amount is less than or equal to the actual data length.
   *
   * This does not modify any actual data in the buffer.
   */
  void trimStart(std::size_t amount) {
    DCHECK_LE(amount, length_);
    data_ += amount;
    length_ -= amount;
  }

  /**
   * Adjust the tail pointer backwards to include less valid data.
   *
   * This moves the tail pointer backwards so that the last amount bytes are no
   * longer considered valid data.  The caller is responsible for ensuring that
   * amount is less than or equal to the actual data length.
   *
   * This does not modify any actual data in the buffer.
   */
  void trimEnd(std::size_t amount) {
    DCHECK_LE(amount, length_);
    length_ -= amount;
  }

  /**
   * Clear the buffer.
   *
   * Postcondition: headroom() == 0, length() == 0, tailroom() == capacity()
   */
  void clear() {
    data_ = mutable_buffer();
    length_ = 0;
  }

  /**
   * Ensure that this buffer has at least minHeadroom headroom bytes and at
   * least minTailroom tailroom bytes.  The buffer must be writable
   * (you must call unshare() before this, if necessary).
   *
   * Postcondition: headroom() >= minHeadroom, tailroom() >= minTailroom,
   * the data (between data() and data() + length()) is preserved.
   */
  void reserve(std::size_t minHeadroom, std::size_t minTailroom) {
    // Maybe we don't need to do anything.
    if (headroom() >= minHeadroom && tailroom() >= minTailroom) {
      return;
    }
    // If the buffer is empty but we have enough total room (head + tail),
    // move the data_ pointer around.
    if (length() == 0 && headroom() + tailroom() >= minHeadroom + minTailroom) {
      data_ = mutable_buffer() + minHeadroom;
      return;
    }
    // Bah, we have to do actual work.
    reserveSlow(minHeadroom, minTailroom);
  }

  /**
   * Return a new IOBuf chain sharing the same data as this chain.
   */
  std::unique_ptr<IOBuf> clone() const;

  /**
   * Similar to clone(). But returns IOBuf by value rather than heap-allocating
   * it.
   */
  IOBuf cloneAsValue() const;

private:
  /**
   * Create a new IOBuf pointing to an external buffer.
   *
   * The caller is responsible for holding a reference count for this new
   * IOBuf.  The IOBuf constructor does not automatically increment the
   * reference count.
   */
  struct InternalConstructor {}; // avoid conflicts
  IOBuf(InternalConstructor, uint8_t *buf, std::size_t capacity, uint8_t *data,
        std::size_t length) noexcept;

private:
  uint8_t *buf_;
  uint8_t *data_;
  size_t length_;
  size_t capacity_;
};

inline std::unique_ptr<IOBuf> IOBuf::copyBuffer(const void *data,
                                                std::size_t size,
                                                std::size_t headroom,
                                                std::size_t minTailroom) {
  std::size_t capacity = headroom + size + minTailroom;
  std::unique_ptr<IOBuf> buf = create(capacity);
  buf->advance(headroom);
  if (size != 0) {
    memcpy(buf->mutable_data(), data, size);
  }
  buf->append(size);
  return buf;
}

inline std::unique_ptr<IOBuf> IOBuf::copyBuffer(const std::string &buf,
                                                std::size_t headroom,
                                                std::size_t minTailroom) {
  return copyBuffer(buf.data(), buf.size(), headroom, minTailroom);
}

#endif // H_CORE_IOBUF
