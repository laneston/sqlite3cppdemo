#include <iostream>
#include "log_manager.hpp"
#include "modbus_message_queue.h"
#include "mqttclient.h"




int main()
{
  // 创建容量为3的消息队列
  MessageQueue mq(3);

  // 构造示例报文（基于提供的JSON数据）
  ModbusMasterMsg msg1;
  msg1.id = 123456;
  msg1.channel = 2;
  msg1.pdu_addr = 5;
  msg1.pdu_func = 3;
  msg1.pdu_data = "0C 00 DC 01 C8 00 DC 00 94 00 DC 02 18 ";
  msg1.timestamp = "20260325180500";

  // 添加寄存器映射
  ModbusMasterMsg::RegisterItem item1{ 16, 220, "A相电压高位" };
  ModbusMasterMsg::RegisterItem item2{ 17, 456, "A相电压低位" };
  ModbusMasterMsg::RegisterItem item3{ 18, 220, "B相电压高位" };
  msg1.register_map.push_back(item1);
  msg1.register_map.push_back(item2);
  msg1.register_map.push_back(item3);

  // 写入队列
  mq.write(msg1);
  std::cout << "写入一条消息，空闲大小: " << mq.freeSize() << std::endl;

  // 再写两条消息（示例简单复制并修改ID）
  ModbusMasterMsg msg2 = msg1;
  msg2.id = 123457;
  ModbusMasterMsg msg3 = msg1;
  msg3.id = 123458;
  mq.write(msg2);
  mq.write(msg3);
  std::cout << "写入三条消息后，空闲大小: " << mq.freeSize() << std::endl;

  // 写入第四条消息（队列容量为3，将覆盖最早消息msg1）
  ModbusMasterMsg msg4 = msg1;
  msg4.id = 123459;
  mq.write(msg4);
  std::cout << "写入第四条（覆盖）后，空闲大小: " << mq.freeSize() << std::endl;

  // 2. 初始化MQTT客户端
  MqttClient client("demo_client");
  client.setServer("localhost", 1883);
  // 如果需要认证，取消下面注释
  // client.setCredentials("username", "password");

  client.setConnectCallback([]() {
    std::cout << "Custom connect callback: ready to publish/subscribe" << std::endl;
    LOG_INFO("MQTT Broker 连接成功.");
  });
  client.setMessageCallback([](const std::string& topic, const std::string& payload) {
    std::cout << "Received message on topic " << topic << ": " << payload << std::endl;
  });

  if (!client.connect()) {
    std::cerr << "Initial connection failed, will retry in state machine." << std::endl;
    LOG_ERROR("MQTT 客户端初始化失败，即将重试!");
  }

  client.subscribe("modbusMaster/database/data", 0);

  std::thread run_thread([&client]() { client.run(); });

  // 读取所有消息
  ModbusMasterMsg out;
  while (mq.read(out)) { std::cout << "读取消息 ID: " << out.id << std::endl; }
  std::cout << "读取完毕，队列大小: " << mq.size() << std::endl;

  client.stop();
  run_thread.join();
  client.disconnect();
  LOG_INFO("MQTT客户端已退出.");

  return 0;
}