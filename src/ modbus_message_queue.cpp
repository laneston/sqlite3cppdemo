// modbus_message_queue.cpp
#include <algorithm>
#include "modbus_message_queue.h"

MessageQueue::MessageQueue(size_t capacity) : buffer_(capacity), head_(0), tail_(0), count_(0)
{
  // 确保容量至少为1
  if (capacity == 0) { buffer_.resize(1); }
}

bool MessageQueue::write(const ModbusMasterMsg& msg)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == buffer_.size()) {
    // 队列已满，覆盖最早节点（head位置）
    buffer_[head_] = msg;
    // 移动head和tail（tail始终指向下一个可写位置）
    head_ = (head_ + 1) % buffer_.size();
    tail_ = (tail_ + 1) % buffer_.size();
    // count_不变
  } else {
    // 有空闲位置，正常写入
    buffer_[tail_] = msg;
    tail_ = (tail_ + 1) % buffer_.size();
    ++count_;
  }
  return true;
}

bool MessageQueue::read(ModbusMasterMsg& msg)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (count_ == 0) {
    return false; // 队列空
  }
  msg = buffer_[head_];
  // 可选：将已读位置的对象重置为默认状态（节省内存非必须）
  // buffer_[head_] = ModbusMasterMsg();
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
  // 重置所有索引，不清空vector内容（后续写入会覆盖）
  head_ = 0;
  tail_ = 0;
  count_ = 0;
  // 可选：将buffer中的对象重置为默认状态
  std::fill(buffer_.begin(), buffer_.end(), ModbusMasterMsg());
}