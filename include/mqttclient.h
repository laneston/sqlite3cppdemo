#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <functional>
#include <string>
#include <vector>

class MqttClient {
public:
  // 回调函数类型
  using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
  using ConnectCallback = std::function<void()>;

  explicit MqttClient(const std::string& client_id);
  ~MqttClient();

  // 设置服务器地址和端口
  bool setServer(const std::string& host, int port);
  // 设置用户名密码（可选）
  bool setCredentials(const std::string& username, const std::string& password);
  // 设置回调
  void setMessageCallback(MessageCallback cb);
  void setConnectCallback(ConnectCallback cb);
  // 连接
  bool connect();
  // 断开连接
  void disconnect();
  // 订阅主题
  bool subscribe(const std::string& topic, int qos = 0);
  // 取消订阅
  bool unsubscribe(const std::string& topic);
  // 发布消息
  bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);
  // 运行状态机：循环检测连接状态，断线自动重连
  void run();  // 阻塞运行，直到调用 stop()
  void stop(); // 停止 run() 循环

  // 获取当前连接状态
  bool isConnected() const;

private:
  struct mosquitto* mosq_ = nullptr;
  std::string host_;
  int port_ = 1883;
  std::string username_;
  std::string password_;
  bool have_credentials_ = false;

  // 订阅列表，用于重连后重新订阅
  struct Subscription {
    std::string topic;
    int qos;
  };
  std::vector<Subscription> subscriptions_;

  // 用户回调
  MessageCallback msg_cb_;
  ConnectCallback conn_cb_;

  // 状态标志
  volatile bool connected_ = false;
  volatile bool running_ = false;

  // 静态回调函数（供 mosquitto 调用）
  static void on_connect(struct mosquitto* mosq, void* obj, int rc);
  static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);
  static void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);

  // 内部重连和重新订阅
  bool reconnect();
  void resubscribe();
};

#endif // MQTTCLIENT_H