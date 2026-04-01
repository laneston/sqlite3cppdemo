#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include "dbManager.h"
#include "json.hpp" // 使用 nlohmann/json 库
#include "log_manager.hpp"
#include "modbus_message_queue.h"
#include "mqttclient.h"

using json = nlohmann::json;

std::atomic<bool> g_running{ true };

// 解析 JSON 为 ModbusMasterMsg，严格按照新 RegisterItem 结构
bool parseModbusMsg(const json& j, ModbusMasterMsg& msg)
{
  try {
    // 基本字段校验
    if (!j.contains("id") || !j["id"].is_number_integer()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.id = j["id"].get<int>();

    if (!j.contains("channel") || !j["channel"].is_number_integer()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.channel = j["channel"].get<int>();

    if (!j.contains("pdu_addr") || !j["pdu_addr"].is_number_integer()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.pdu_addr = j["pdu_addr"].get<int>();

    if (!j.contains("pdu_func") || !j["pdu_func"].is_number_integer()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.pdu_func = j["pdu_func"].get<int>();

    if (!j.contains("pdu_data") || !j["pdu_data"].is_string()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.pdu_data = j["pdu_data"].get<std::string>();

    if (!j.contains("timestamp") || !j["timestamp"].is_string()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.timestamp = j["timestamp"].get<std::string>();

    // register_map 解析（新结构：每个对象包含三个键）
    if (!j.contains("register_map") || !j["register_map"].is_array()) {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
    msg.register_map.clear();
    for (const auto& item : j["register_map"]) {
      if (!item.is_object()) {
        printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
        return false;
      }

      // 提取地址键名（如 "16"）和映射地址值（如 0）
      int address = -1, map_addr = -1, value = -1;
      std::string description;

      // 遍历对象中的键值对
      for (auto it = item.begin(); it != item.end(); ++it) {
        const std::string& key = it.key();
        if (key == "value") {
          if (!it.value().is_number_integer()) {
            printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
            return false;
          }
          value = it.value().get<int>();
        } else if (key == "description") {
          if (!it.value().is_string()) {
            printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
            return false;
          }
          description = it.value().get<std::string>();
        } else {
          // 其他键视为地址键名，其值应为 map_addr
          if (!it.value().is_number_integer()) {
            printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
            return false;
          }
          address = std::stoi(key);
          map_addr = it.value().get<int>();
        }
      }

      // 必须同时包含 address, map_addr, value, description
      if (address == -1 || map_addr == -1 || value == -1 || description.empty()) {
        {
          printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
          return false;
        }
      }

      ModbusMasterMsg::RegisterItem reg;
      reg.address = address;
      reg.map_addr = map_addr;
      reg.value = value;
      reg.description = description;
      msg.register_map.push_back(reg);
    }
    return true;
  } catch (const std::exception& e) {
    LOG_WARN("JSON parse exception: " + std::string(e.what()));
    {
      printf("error file: %s, line num: %d\n", __FILE__, __LINE__);
      return false;
    }
  }
}

int main()
{
  // 初始化日志（可调整参数）
  if (!LogManager::getInstance().init(LogLevel::INFO, "/root/log/database.log", 1024 * 1024, 3600, 10)) {
    std::cerr << "LogManager init failed!" << std::endl;
    return -1;
  }
  LOG_INFO("Application started.");

  DataBaseManager dbManager(2, 10); // 最多2个通道，每个数据库最多10张表

  // 创建消息队列（容量 100）
  MessageQueue mq(100);

  // 配置 MQTT 客户端
  MqttClient client("database_reader");
  client.setServer("localhost", 1883); // 按实际地址修改
  // client.setCredentials("username", "password");

  client.setConnectCallback([]() { LOG_INFO("Connected to MQTT broker."); });

  // 消息回调：解析并写入队列
  client.setMessageCallback([&mq](const std::string& topic, const std::string& payload) {
    LOG_DEBUG("Received message on topic: " + topic);
    try {
      json j = json::parse(payload);
      ModbusMasterMsg msg;
      if (parseModbusMsg(j, msg)) {
        mq.write(msg);
        LOG_INFO("Enqueued msg id=" + std::to_string(msg.id)
                 + ", registers=" + std::to_string(msg.register_map.size()));
      } else {
        LOG_WARN("Invalid message format, discarded.");
      }
    } catch (const json::parse_error& e) {
      LOG_WARN("JSON parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
      LOG_WARN("Unexpected error: " + std::string(e.what()));
    }
  });

  if (!client.connect()) { LOG_ERROR("MQTT initial connection failed, will retry in run loop."); }
  client.subscribe("modbusMaster/database/data", 0);

  // MQTT 运行线程
  std::thread mqtt_thread([&client]() { client.run(); });

  // 定时读取线程（每 60 秒清空队列并打印）
  std::thread reader_thread([&mq, &dbManager]() {
    while (g_running) {
      std::this_thread::sleep_for(std::chrono::seconds(60));
      if (!g_running) break;

      size_t count = mq.size();
      LOG_INFO("Queue size before reading: " + std::to_string(count));
      ModbusMasterMsg msg;
      while (mq.read(msg)) {
        // 写入数据库
        if (!dbManager.writeMessage(msg)) { LOG_ERROR("DB write failed for msg id=" + std::to_string(msg.id)); }
        std::ostringstream oss;
        oss << "Read msg id=" << msg.id << ", channel=" << msg.channel << ", pdu_addr=" << msg.pdu_addr
            << ", pdu_func=" << msg.pdu_func << ", pdu_data=" << msg.pdu_data << ", timestamp=" << msg.timestamp
            << ", registers_count=" << msg.register_map.size();
        LOG_INFO(oss.str());

        // 可选：打印每个寄存器详情
        for (const auto& reg : msg.register_map) {
          LOG_INFO("  reg: address=" + std::to_string(reg.address) + ", map_addr=" + std::to_string(reg.map_addr)
                   + ", value=" + std::to_string(reg.value) + ", desc=" + reg.description);
        }
      }
      LOG_INFO("Queue reading completed.");
    }
    LOG_INFO("Reader thread exiting.");
  });

  LOG_INFO("Application is running. Press Ctrl+C to exit.");
  while (g_running) { std::this_thread::sleep_for(std::chrono::seconds(1)); }

  // 优雅退出
  LOG_INFO("Stopping application...");
  client.stop();
  mqtt_thread.join();
  g_running = false;
  reader_thread.join();

  LOG_INFO("Application exited.");
  return 0;
}