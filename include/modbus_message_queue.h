#ifndef MODBUS_MESSAGE_QUEUE_H
#define MODBUS_MESSAGE_QUEUE_H

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

// 报文内容结构体，对应JSON格式
struct ModbusMasterMsg {
  int id;                // 消息ID
  int channel;           // 通道号
  int pdu_addr;          // PDU地址
  int pdu_func;          // 功能码
  std::string pdu_data;  // 十六进制数据字符串，如 "0C 00 DC ..."
  std::string timestamp; // 时间戳 "YYYYMMDDHHMMSS"

  // 寄存器映射条目
  struct RegisterItem {
    int address;             // 寄存器地址（如16,17...）
    int value;               // 寄存器值
    std::string description; // 描述信息
  };
  std::vector<RegisterItem> register_map;

  // 默认构造函数
  ModbusMasterMsg() : id(0), channel(0), pdu_addr(0), pdu_func(0) {}

  // 方便构造的构造函数（可选）
  ModbusMasterMsg(int i, int ch, int addr, int func, const std::string& data, const std::string& ts)
    : id(i), channel(ch), pdu_addr(addr), pdu_func(func), pdu_data(data), timestamp(ts)
  {
  }
};

// 环形消息队列类（线程安全）
class MessageQueue {
public:
  // 构造函数：指定队列容量（大小固定）
  explicit MessageQueue(size_t capacity);

  // 禁止拷贝和赋值
  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  // 写函数：将报文写入队列，队列满时覆盖最早节点
  bool write(const ModbusMasterMsg& msg);

  // 读函数：读取最早报文并移除，成功返回true，队列空返回false
  bool read(ModbusMasterMsg& msg);

  // 查询空闲队列大小（未使用的节点数）
  size_t freeSize() const;

  // 查询当前队列中的报文数量
  size_t size() const;

  // 清空队列（可选）
  void clear();

private:
  std::vector<ModbusMasterMsg> buffer_; // 环形缓冲区
  size_t head_;                         // 指向最早有效节点
  size_t tail_;                         // 指向下一个可写位置
  size_t count_;                        // 当前有效节点数
  mutable std::mutex mutex_;            // 互斥锁（保护所有成员）
};

#endif // MODBUS_MESSAGE_QUEUE_H