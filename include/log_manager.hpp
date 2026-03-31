#ifndef __LOG_MANAGER_HPP
#define __LOG_MANAGER_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

// 日志等级定义（C++11枚举类，类型安全）
enum class LogLevel {
  DEBUG = 0, // 调试信息
  INFO = 1,  // 普通信息
  WARN = 2,  // 警告
  ERROR = 3, // 错误
  FATAL = 4  // 致命错误
};

// 文件信息结构体（用于日志文件排序）
struct LogFileInfo {
  std::string filename; // 文件名
  time_t modify_time;   // 修改时间
};

// 日志管理器（单例模式，全局唯一）
class LogManager {
public:
  // 获取单例实例（C++11线程安全的局部静态变量）
  static LogManager& getInstance()
  {
    static LogManager instance;
    return instance;
  }

  // 初始化日志配置（新增max_log_files参数：最大日志文件数量）
  bool init(LogLevel level, const std::string& log_path, uint64_t size_threshold, uint64_t time_interval,
            uint32_t max_log_files)
  {
    std::lock_guard<std::mutex> lock(log_mutex);

    // 校验参数
    if (log_path.empty() || size_threshold == 0 || time_interval == 0 || max_log_files == 0) {
      std::cerr << "Invalid log init parameters!" << std::endl;
      return false;
    }

    // 解析日志路径（分离目录和文件名）
    size_t last_slash = log_path.find_last_of('/');
    if (last_slash == std::string::npos) {
      log_dir = ".";            // 默认当前目录
      log_base_name = log_path; // 基础文件名
    } else {
      log_dir = log_path.substr(0, last_slash);
      log_base_name = log_path.substr(last_slash + 1);
    }

    // 初始化配置
    current_level = level;
    this->size_threshold = size_threshold;
    this->time_interval = time_interval;
    this->max_log_files = max_log_files;

    // 确保日志目录存在
    if (!createDirectory(log_dir)) {
      std::cerr << "Failed to create log directory: " << log_dir << std::endl;
      return false;
    }

    // 打开初始日志文件
    if (!openNewLogFile()) {
      std::cerr << "Failed to open initial log file!" << std::endl;
      return false;
    }

    // 初始化时清理超出数量的旧日志
    cleanupOldLogs();

    std::cout << "LogManager init success! " << "Level: " << getLogLevelString(current_level) << ", "
              << "Size threshold: " << size_threshold << " bytes, " << "Time interval: " << time_interval
              << " seconds, " << "Max log files: " << max_log_files << std::endl;
    return true;
  }

  // 日志打印接口（核心函数）
  void log(LogLevel level, const std::string& message)
  {
    // 等级过滤：低于当前等级的日志不打印
    if (level < current_level) { return; }

    std::lock_guard<std::mutex> lock(log_mutex); // 线程安全

    // 检查是否需要轮转
    if (checkRotateCondition()) { rotateLog(); }

    // 生成日志内容（时间 + 等级 + 消息）
    auto now = std::chrono::system_clock::now();
    std::string time_str = getCurrentTimeString(now);
    std::string level_str = getLogLevelString(level);
    std::string log_msg = "[" + time_str + "] [" + level_str + "] " + message + "\n";

    // 写入文件并更新当前文件大小
    if (log_file.is_open()) {
      log_file << log_msg;
      log_file.flush(); // 立即刷盘，避免缓存丢失
      current_file_size += log_msg.size();
    }
  }

// 便捷的日志宏封装（简化调用）
#define LOG_DEBUG(msg) LogManager::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg) LogManager::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARN(msg) LogManager::getInstance().log(LogLevel::WARN, msg)
#define LOG_ERROR(msg) LogManager::getInstance().log(LogLevel::ERROR, msg)
#define LOG_FATAL(msg) LogManager::getInstance().log(LogLevel::FATAL, msg)

private:
  // 私有构造/析构（单例模式）
  LogManager() : current_level(LogLevel::INFO), current_file_size(0), max_log_files(10) {}
  ~LogManager()
  {
    if (log_file.is_open()) { log_file.close(); }
  }

  // 禁止拷贝赋值
  LogManager(const LogManager&) = delete;
  LogManager& operator=(const LogManager&) = delete;

  // 创建目录（递归创建，兼容多级目录）
  bool createDirectory(const std::string& dir)
  {
    if (dir.empty() || dir == ".") return true;

    struct stat st;
    if (stat(dir.c_str(), &st) == 0) {
      return S_ISDIR(st.st_mode); // 目录已存在
    }

    // 递归创建父目录
    size_t pos = dir.find_last_of('/');
    if (pos != std::string::npos) {
      std::string parent_dir = dir.substr(0, pos);
      if (!createDirectory(parent_dir)) { return false; }
    }

    // 创建当前目录（权限755）
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
      std::cerr << "mkdir failed: " << dir << ", errno: " << errno << std::endl;
      return false;
    }
    return true;
  }

  // 打开新的日志文件
  bool openNewLogFile()
  {
    // 关闭旧文件
    if (log_file.is_open()) { log_file.close(); }

    // 拼接完整日志路径
    std::string full_log_path = log_dir + "/" + log_base_name;

    // 打开新文件（追加模式）
    log_file.open(full_log_path, std::ios::out | std::ios::app);
    if (!log_file.is_open()) {
      std::cerr << "Failed to open log file: " << full_log_path << std::endl;
      return false;
    }

    // 初始化文件属性
    file_create_time = std::chrono::system_clock::now();
    current_file_size = 0;

    // 获取文件当前大小（如果文件已存在）
    log_file.seekp(0, std::ios::end);
    current_file_size = log_file.tellp();

    return true;
  }

  // 检查轮转条件（大小超限 或 时间超限）
  bool checkRotateCondition()
  {
    if (!log_file.is_open()) { return false; }

    // 1. 检查文件大小
    bool size_exceed = (current_file_size >= size_threshold);

    // 2. 检查时间间隔
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - file_create_time);
    bool time_exceed = (duration.count() >= time_interval);

    return size_exceed || time_exceed;
  }

  // 压缩日志文件为.tar.gz（系统调用tar命令）
  bool compressLogFile(const std::string& src_file)
  {
    if (src_file.empty() || access(src_file.c_str(), F_OK) != 0) {
      std::cerr << "Source log file not exist: " << src_file << std::endl;
      return false;
    }

    // 压缩命令：tar -zcf 目标文件 源文件 && rm 源文件
    std::string dst_file = src_file + ".tar.gz";
    std::string cmd = "tar -zcf " + dst_file + " " + src_file + " && rm -f " + src_file;

    // 执行压缩命令（popen/pclose兼容POSIX，支持ARM Linux）
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
      std::cerr << "Failed to execute compress command: " << cmd << std::endl;
      return false;
    }

    // 等待命令执行完成
    int ret = pclose(fp);
    if (ret != 0) {
      std::cerr << "Compress command failed, ret: " << ret << ", cmd: " << cmd << std::endl;
      return false;
    }

    std::cout << "Log file compressed: " << dst_file << std::endl;
    return true;
  }

  // 扫描日志目录下的压缩日志文件，返回按修改时间排序的列表（旧->新）
  std::vector<LogFileInfo> scanLogFiles()
  {
    std::vector<LogFileInfo> log_files;
    DIR* dir = opendir(log_dir.c_str());
    if (dir == nullptr) {
      std::cerr << "Failed to open log directory: " << log_dir << std::endl;
      return log_files;
    }

    struct dirent* entry;
    std::string prefix = log_base_name + "."; // 日志文件前缀（如app.log.20260323...）
    std::string suffix = ".tar.gz";           // 压缩文件后缀

    while ((entry = readdir(dir)) != nullptr) {
      std::string filename = entry->d_name;
      // 过滤出符合命名规则的压缩日志文件
      if (filename.find(prefix) == 0 && filename.find(suffix) != std::string::npos) {
        std::string full_path = log_dir + "/" + filename;
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) { log_files.push_back({ filename, st.st_mtime }); }
      }
    }
    closedir(dir);

    // 按修改时间升序排序（最老的在前）
    std::sort(log_files.begin(), log_files.end(),
              [](const LogFileInfo& a, const LogFileInfo& b) { return a.modify_time < b.modify_time; });

    return log_files;
  }

  // 清理超出数量阈值的最老日志文件
  void cleanupOldLogs()
  {
    std::vector<LogFileInfo> log_files = scanLogFiles();
    if (log_files.size() <= max_log_files) {
      return; // 未超过阈值，无需清理
    }

    // 需要删除的文件数量
    uint32_t delete_count = log_files.size() - max_log_files;
    std::cout << "Log files exceed max count (" << log_files.size() << "/" << max_log_files << "), will delete "
              << delete_count << " old files" << std::endl;

    // 删除最老的文件
    for (uint32_t i = 0; i < delete_count; ++i) {
      std::string full_path = log_dir + "/" + log_files[i].filename;
      if (unlink(full_path.c_str()) == 0) {
        std::cout << "Deleted old log file: " << full_path << std::endl;
      } else {
        std::cerr << "Failed to delete old log file: " << full_path << std::endl;
      }
    }
  }

  // 执行日志轮转（新增压缩+清理逻辑）
  void rotateLog()
  {
    // 关闭当前文件
    log_file.close();

    // 生成轮转后的文件名（基础名 + 时间戳）
    std::string rotate_filename = log_dir + "/" + log_base_name + "." + getCurrentTimeString();
    std::string full_log_path = log_dir + "/" + log_base_name;

    // 重命名当前文件
    if (std::rename(full_log_path.c_str(), rotate_filename.c_str()) != 0) {
      std::cerr << "Failed to rotate log file: " << full_log_path << std::endl;
    } else {
      std::cout << "Log rotated to: " << rotate_filename << std::endl;

      // 轮转后压缩文件
      compressLogFile(rotate_filename);

      // 压缩后清理超出数量的旧日志
      cleanupOldLogs();
    }

    // 打开新的日志文件
    openNewLogFile();
  }

  // 获取格式化的时间字符串（YYYYMMDDHHMMSS）
  std::string getCurrentTimeString(std::chrono::system_clock::time_point tp = std::chrono::system_clock::now())
  {
    std::time_t now_c = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_now;
    // 线程安全的本地时间转换（兼容POSIX系统，如Linux）
    localtime_r(&now_c, &tm_now);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << tm_now.tm_year + 1900 // 年
        << std::setw(2) << tm_now.tm_mon + 1                          // 月
        << std::setw(2) << tm_now.tm_mday                             // 日
        << std::setw(2) << tm_now.tm_hour                             // 时
        << std::setw(2) << tm_now.tm_min                              // 分
        << std::setw(2) << tm_now.tm_sec;                             // 秒
    return oss.str();
  }

  // 日志等级转字符串
  std::string getLogLevelString(LogLevel level)
  {
    switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARN: return "WARN";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::FATAL: return "FATAL";
    default: return "UNKNOWN";
    }
  }

  // 成员变量
  LogLevel current_level;                                 // 当前日志等级
  std::string log_dir;                                    // 日志目录
  std::string log_base_name;                              // 日志基础文件名
  uint64_t size_threshold;                                // 大小阈值（字节）
  uint64_t time_interval;                                 // 时间周期（秒）
  uint32_t max_log_files;                                 // 最大日志文件数量（压缩后）
  std::ofstream log_file;                                 // 当前日志文件流
  std::mutex log_mutex;                                   // 日志互斥锁（线程安全）
  std::chrono::system_clock::time_point file_create_time; // 文件创建时间
  uint64_t current_file_size;                             // 当前文件大小（字节）
};

#endif // __LOG_MANAGER_HPP

/*-----------------------------------------------------------------------------------
// 主函数（测试示例）
int main() {
  // 1. 初始化日志管理器
  // 参数说明：
  //   日志等级：DEBUG（打印所有等级）s
  //   日志路径："./app.log"（当前目录下的app.log）
  //   大小阈值：1024*100 = 100KB（方便测试轮转）
  //   时间周期：30秒（方便测试时间轮转）
  //   最大日志文件数：5（超过则删除最老的）
  if (!LogManager::getInstance().init(LogLevel::DEBUG, "./app.log", 1024 * 100,
                                      30, 5)) {
    std::cerr << "LogManager init failed!" << std::endl;
    return -1;
  }

  // 2. 模拟日志打印（测试轮转+压缩+清理）
  LOG_INFO("===== LogManager Demo Start =====");
  LOG_DEBUG("This is a DEBUG log");
  LOG_INFO("This is an INFO log");
  LOG_WARN("This is a WARN log");
  LOG_ERROR("This is an ERROR log");
  LOG_FATAL("This is a FATAL log");

  // 循环打印日志，触发多次大小轮转（测试压缩和数量清理）
  for (int i = 0; i < 10000; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ostringstream oss;
    oss << "Test log line " << i << " (trigger size rotate and compress)";
    LOG_INFO(oss.str());

    // 每2000行打印一次状态，方便观察
    if (i % 2000 == 0) {
      LOG_INFO("Current log line count: " + std::to_string(i));
    }
  }

  // 等待时间轮转（测试时间触发的轮转+压缩）
  LOG_INFO("Waiting for time rotate...");
  std::this_thread::sleep_for(std::chrono::seconds(31));
  LOG_INFO("Time rotate triggered, log will be compressed");

  // 继续打印日志，触发数量清理
  for (int i = 0; i < 5000; ++i) {
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::ostringstream oss;
    oss << "Test log line (trigger cleanup) " << i;
    LOG_INFO(oss.str());
  }

  LOG_INFO("===== LogManager Demo End =====");

  return 0;
}
----------------------------------------------------------------*/