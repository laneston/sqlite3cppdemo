#ifndef ENV_DATA_MANAGER_HPP
#define ENV_DATA_MANAGER_HPP

#include <ctime>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "sqlite3.h"

/// @brief 数据存储结构
struct EnvData {
  time_t timestamp;
  float temperature;
  float humidity;
  int device_id;

  EnvData(float temp, float hum, int dev_id) : temperature(temp), humidity(hum), device_id(dev_id)
  {
    timestamp = time(nullptr);
  }
  EnvData() : timestamp(0), temperature(0.0f), humidity(0.0f), device_id(0) {}
};

/// @brief 数据库操作类
class EnvDataManager {
public:
  // 获取单例实例
  static EnvDataManager& getInstance()
  {
    static EnvDataManager instance;
    return instance;
  }

  // 获取设备序列号
  std::string getDeviceSN() const { return device_sn_; }

  /// @brief 根据当日的时间戳获取表名
  /// @param timestamp 时间戳
  /// @return 表明字符串
  std::string getTableName(time_t timestamp)
  {
    struct tm* tm_info = localtime(&timestamp);
    char buffer[9];
    strftime(buffer, sizeof(buffer), "%Y%m%d", tm_info);
    return std::string(buffer);
  }

  /// @brief 初始化数据库
  /// @param device_sn 数据库文件名称
  /// @return 函数执行成功或失败的状态
  bool initDatabase(const std::string& device_sn)
  {
    device_sn_ = device_sn;
    std::string db_filename = device_sn + ".db"; // 构建数据库文件名

    db_mutex_.lock();                                 // 互斥锁上锁
    int rc = sqlite3_open(db_filename.c_str(), &db_); // 打开或创建数据库
    if (rc != SQLITE_OK) {
      std::cerr << "无法打开数据库 " << db_filename << ": " << sqlite3_errmsg(db_) << std::endl;
      if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
      }
      db_mutex_.unlock(); // 互斥锁解锁
      return false;
    }
    std::cout << "数据库 " << db_filename << " 打开成功" << std::endl;
    // 启用外键约束
    executeSQL("PRAGMA foreign_keys = ON;");
    db_mutex_.unlock();
    // 创建今天的数据表
    return createTodayTable();
  }

  /// @brief 获取所有表名
  /// @return 表名容器
  std::vector<std::string> getAllTables()
  {
    std::vector<std::string> tables;
    if (!db_) { return tables; }
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE '2_______' ORDER BY name;";

    db_mutex_.lock(); // 互斥锁上锁
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
      db_mutex_.unlock();
      return tables;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char* table_name = sqlite3_column_text(stmt, 0);
      if (table_name) { tables.push_back(reinterpret_cast<const char*>(table_name)); }
    }
    sqlite3_finalize(stmt);

    db_mutex_.unlock();
    return tables;
  }

  /// @brief 清理旧表格
  /// @return 函数执行成功或失败的状态
  bool cleanupOldTable()
  {
    auto tables = getAllTables();

    // 如果表数量超过7，删除最旧的表
    if (tables.size() > 7) {
      int tables_to_delete = tables.size() - 7;
      for (int i = 0; i < tables_to_delete; i++) {
        std::string drop_sql = "DROP TABLE IF EXISTS " + tables[i] + ";";
        if (!executeSQL(drop_sql)) {
          std::cerr << "删除表 " << tables[i] << " 失败" << std::endl;
          return false;
        }
        std::cout << "删除旧表: " << tables[i] << std::endl;
      }
    }

    return true;
  }

  /// @brief 插入环境数据
  /// @param data 数据存储结构
  /// @return 函数执行成功或失败的状态
  bool insertEnvData(const EnvData& data)
  {
    // 检查是否需要创建新表
    if (!checkAndCreateNewTable()) { return false; }

    std::stringstream sql;
    sql << "INSERT INTO \"" << current_table_ << "\" (timestamp, temperature, humidity, device_id) VALUES ("
        << data.timestamp << ", " << data.temperature << ", " << data.humidity << ", " << data.device_id << ");";

    return executeSQL(sql.str());
  }

  // 析构函数
  ~EnvDataManager()
  {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (db_) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
  }

private:
  // SQLite数据库指针
  sqlite3* db_;
  std::string device_sn_;
  std::string current_table_;
  std::mutex db_mutex_;

  // 最后检查日期
  int last_check_day_;

  EnvDataManager() : db_(nullptr), last_check_day_(-1) {} // 私有构造函数

  // 禁用拷贝和赋值
  EnvDataManager(const EnvDataManager&) = delete;
  EnvDataManager& operator=(const EnvDataManager&) = delete;

  /// @brief 执行SQL语句
  /// @param sql SQL语句
  /// @return 函数执行成功或失败的状态
  bool executeSQL(const std::string& sql)
  {
    if (!db_) {
      std::cerr << "数据库未初始化" << std::endl;
      return false;
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
      std::cerr << "SQL执行错误: " << (err_msg ? err_msg : "未知错误") << std::endl;
      std::cerr << "SQL语句: " << sql << std::endl;
      if (err_msg) { sqlite3_free(err_msg); }
      return false;
    }

    return true;
  }

  /// @brief 创建当天表
  /// @return 函数执行成功或失败的状态
  bool createTodayTable()
  {
    time_t now = time(nullptr);
    current_table_ = getTableName(now);
    std::stringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS \"" << current_table_ << "\" (" << "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        << "timestamp INTEGER NOT NULL," << "temperature REAL NOT NULL," << "humidity REAL NOT NULL,"
        << "device_id INTEGER NOT NULL," << "created_at DATETIME DEFAULT CURRENT_TIMESTAMP" << ");";
    // 创建索引以提高查询性能
    std::string index_sql
      = "CREATE INDEX IF NOT EXISTS idx_timestamp_" + current_table_ + " ON \"" + current_table_ + "\"(timestamp);";
    db_mutex_.lock(); // 互斥锁上锁
    if (!executeSQL(sql.str())) {
      db_mutex_.unlock();
      return false;
    }
    if (!executeSQL(index_sql)) { std::cerr << "创建索引失败，但表已创建" << std::endl; }
    // 更新最后检查日期
    struct tm* tm_info = localtime(&now);
    last_check_day_ = tm_info->tm_yday;
    std::cout << "表 " << current_table_ << " 创建成功" << std::endl;
    db_mutex_.unlock();
    // 清理旧数据
    cleanupOldTable();
    return true;
  }

  /// @brief 检查是否需要创建新表
  /// @return 函数执行成功或失败的状态
  bool checkAndCreateNewTable()
  {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    int current_day = tm_info->tm_yday;
    int current_year = tm_info->tm_year;

    // 如果日期已变化（新的一天）或者年份变化
    if (current_day != last_check_day_) { // 处理跨年情况
      if (current_year != tm_info->tm_year) {
        last_check_day_ = -1; // 重置
      }

      std::cout << "新的一天，创建新表..." << std::endl;
      return createTodayTable();
    }

    return true;
  }
};

#endif // ENV_DATA_MANAGER_HPP
