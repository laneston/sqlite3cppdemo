#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include "EnvDataManager.hpp"

/// @brief 生成模拟温湿度数据
/// @param device_id 设备ID
/// @return 数据结构对象
EnvData generateMockData(int device_id)
{
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // 温度: 15-30度
  static std::uniform_real_distribution<> temp_dist(15.0, 30.0);
  // 湿度: 30-80%
  static std::uniform_real_distribution<> hum_dist(30.0, 80.0);

  float temperature = static_cast<float>(temp_dist(gen));
  float humidity = static_cast<float>(hum_dist(gen));

  return EnvData(temperature, humidity, device_id);
}

/// @brief 打印环境数据
/// @param data 温湿度数据存储结构
void printEnvData(const EnvData& data)
{
  struct tm* timeinfo = localtime(&data.timestamp);
  char time_str[20];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

  std::cout << "[" << time_str << "] " << "设备: " << data.device_id << ", " << "温度: " << std::fixed
            << std::setprecision(1) << data.temperature << "°C, " << "湿度: " << std::fixed << std::setprecision(1)
            << data.humidity << "%" << std::endl;
}

/// @brief 入口函数
int main()
{
  std::vector<std::string> device_sns = { "SN001", "SN002", "SN003" }; // 模拟多个设备
  static std::map<std::string, bool> initialized;                      // 记录初始化状态

  for (const auto& device_sn : device_sns) {
    EnvDataManager& manager = EnvDataManager::getInstance(); // 获取设备管理器实例

    if (manager.getDeviceSN() != device_sn) { // 如果当前实例的设备SN不同，需要重新初始化
      if (manager.initDatabase(device_sn)) {  // 初始化数据库
        std::cout << "设备 " << device_sn << " 数据库初始化成功" << std::endl;
        initialized[device_sn] = true;
      } else {
        std::cerr << "设备 " << device_sn << " 数据库初始化失败" << std::endl;
        continue;
      }
    } else if (!initialized[device_sn]) { // 如果当前实例还未初始化，但设备SN匹配，则标记为已初始化
    } else {                              // 如果当期实例SN匹配且已初始化，则跳过此操作
      std::cout << "设备 " << device_sn << "匹配且已初始化" << std::endl;
    }
    EnvData data = generateMockData(std::stoi(device_sn.substr(2))); // 为每个设备生成数据
    printEnvData(data);

    if (!manager.insertEnvData(data)) { std::cerr << "设备 " << device_sn << " 数据插入失败" << std::endl; }
  }

  return 0;
}