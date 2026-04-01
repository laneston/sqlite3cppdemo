#include <algorithm>
#include "modbus_message_queue.h"

MessageQueue::MessageQueue(size_t capacity) : buffer_(capacity), head_(0), tail_(0), count_(0)
{
  if (capacity == 0) { buffer_.resize(1); }
}

bool MessageQueue::write(const ModbusMasterMsg& msg)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == buffer_.size()) {
    // 覆盖最早节点
    buffer_[head_] = msg;
    head_ = (head_ + 1) % buffer_.size();
    tail_ = (tail_ + 1) % buffer_.size();
  } else {
    buffer_[tail_] = msg;
    tail_ = (tail_ + 1) % buffer_.size();
    ++count_;
  }
  return true;
}

bool MessageQueue::read(ModbusMasterMsg& msg)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == 0) return false;
  msg = buffer_[head_];
  head_ = (head_ + 1) % buffer_.size();
  --count_;
  return true;
}

size_t MessageQueue::freeSize() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return buffer_.size() - count_;
}

size_t MessageQueue::size() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return count_;
}

void MessageQueue::clear()
{
  std::lock_guard<std::mutex> lock(mutex_);
  head_ = 0;
  tail_ = 0;
  count_ = 0;
  std::fill(buffer_.begin(), buffer_.end(), ModbusMasterMsg());
}