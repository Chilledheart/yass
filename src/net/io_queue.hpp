// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Chilledheart  */

#ifndef CORE_IO_QUEUE_HPP
#define CORE_IO_QUEUE_HPP

#include <array>
#include <memory>
#include "net/iobuf.hpp"

namespace net {

class IoQueue {
  using T = std::shared_ptr<IOBuf>;
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

  void push_back(const char* data, size_t length) {
    push_back(IOBuf::copyBuffer(data, length));
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

} // namespace net

#endif // CORE_IO_QUEUE_HPP
