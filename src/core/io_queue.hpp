// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef CORE_IO_QUEUE_HPP
#define CORE_IO_QUEUE_HPP

#include <array>
#include <memory>
#include "core/iobuf.hpp"

class IoQueue {
  using T = std::shared_ptr<IOBuf>;
  using PooledT = std::vector<T>;
 public:
  IoQueue() {}
  IoQueue(const IoQueue&) = default;
  IoQueue& operator=(const IoQueue&) = default;

  bool empty() const {
    return idx_ == end_idx_;
  }

  void push_back(T buf) {
    queue_[end_idx_] = buf;
    end_idx_ = (end_idx_ + 1) % queue_.size();
    CHECK_NE(end_idx_, idx_) << "IO queue is full";
  }

  void push_back(const char* data, size_t length, PooledT *pool) {
    std::shared_ptr<IOBuf> buf;
    if (pool && !pool->empty()) {
      buf = pool->back();
      pool->pop_back();
      buf->clear();
      buf->reserve(0, length);
      memcpy(buf->mutable_tail(), data, length);
      buf->append(length);
    } else {
      buf = IOBuf::copyBuffer(data, length);
    }
    push_back(buf);
  }

  bool push_back_merged(T buf, PooledT *pool) {
    DCHECK(!buf->empty());
    if (empty() || (this->length() == 1 && dirty_front_)) {
      push_back(buf);
      return false;
    }
    auto prev_buf = queue_[(end_idx_ + queue_.size() - 1) % queue_.size()];
    prev_buf->reserve(0, buf->length());
    memcpy(prev_buf->mutable_tail(), buf->data(), buf->length());
    prev_buf->append(buf->length());
    if (pool) {
      pool->push_back(buf);
    }
    return true;
  }

  void push_back_merged(const char* data, size_t length, PooledT *pool) {
    DCHECK(data && length);
    // if empty or the only buffer is dirty
    if (empty() || (this->length() == 1 && dirty_front_)) {
      push_back(data, length, pool);
      return;
    }
    auto prev_buf = queue_[(end_idx_ + queue_.size() - 1) % queue_.size()];
    prev_buf->reserve(0, length);
    memcpy(prev_buf->mutable_tail(), data, length);
    prev_buf->append(length);
  }

  T front() {
    DCHECK(!empty());
    dirty_front_ = true;
    return queue_[idx_];
  }

  void pop_front() {
    DCHECK(!empty());
    dirty_front_ = false;
    queue_[idx_] = nullptr;
    idx_ = (idx_ + 1) % queue_.size();
  }

  T back() {
    DCHECK(!empty());
    return queue_[(end_idx_ + queue_.size() - 1) % queue_.size()];
  }

  size_t length() const {
    return (end_idx_ + queue_.size() - idx_) % queue_.size();
  }

  size_t byte_length() const {
    if (empty())  {
      return 0u;
    }
    size_t ret = 0u;
    for (int i = idx_; i != end_idx_; i = (i+1) % queue_.size())
      ret += queue_[i]->length();
    return ret;
  }

 private:
  int idx_ = 0;
  int end_idx_ = 0;
  std::array<T, 4096> queue_;
  bool dirty_front_ = false;
};

#endif // CORE_IO_QUEUE_HPP
