#include "mqttclient.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include "mosquitto.h"

MqttClient::MqttClient(const std::string& client_id)
{
  mosquitto_lib_init();
  mosq_ = mosquitto_new(client_id.c_str(), true, this);
  if (!mosq_) { throw std::runtime_error("Failed to create mosquitto instance"); }
  mosquitto_connect_callback_set(mosq_, on_connect);
  mosquitto_disconnect_callback_set(mosq_, on_disconnect);
  mosquitto_message_callback_set(mosq_, on_message);
}

MqttClient::~MqttClient()
{
  stop();
  if (mosq_) {
    mosquitto_disconnect(mosq_);
    mosquitto_destroy(mosq_);
  }
  mosquitto_lib_cleanup();
}

bool MqttClient::setServer(const std::string& host, int port)
{
  host_ = host;
  port_ = port;
  return true;
}

bool MqttClient::setCredentials(const std::string& username, const std::string& password)
{
  username_ = username;
  password_ = password;
  have_credentials_ = true;
  return true;
}

void MqttClient::setMessageCallback(MessageCallback cb) { msg_cb_ = cb; }

void MqttClient::setConnectCallback(ConnectCallback cb) { conn_cb_ = cb; }

bool MqttClient::connect()
{
  if (!mosq_) return false;

  if (have_credentials_) {
    int rc = mosquitto_username_pw_set(mosq_, username_.c_str(), password_.c_str());
    if (rc != MOSQ_ERR_SUCCESS) return false;
  }

  int rc = mosquitto_connect(mosq_, host_.c_str(), port_, 60);
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "Connect error: " << mosquitto_strerror(rc) << std::endl;
    return false;
  }

  rc = mosquitto_loop_start(mosq_);
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "Loop start error: " << mosquitto_strerror(rc) << std::endl;
    return false;
  }

  // 等待连接成功回调（最多5秒）
  for (int i = 0; i < 50 && !connected_; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
  return connected_;
}

void MqttClient::disconnect()
{
  if (mosq_) {
    mosquitto_disconnect(mosq_);
    mosquitto_loop_stop(mosq_, true);
    connected_ = false;
  }
}

bool MqttClient::subscribe(const std::string& topic, int qos)
{
  if (!mosq_) return false;
  // 保存订阅信息，以便重连后重新订阅
  subscriptions_.push_back({ topic, qos });
  if (connected_) {
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
    if (rc != MOSQ_ERR_SUCCESS) {
      std::cerr << "Subscribe error: " << mosquitto_strerror(rc) << std::endl;
      subscriptions_.pop_back(); // 失败则移除记录
      return false;
    }
  }
  return true;
}

bool MqttClient::unsubscribe(const std::string& topic)
{
  // 从订阅列表中移除
  for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
    if (it->topic == topic) {
      subscriptions_.erase(it);
      break;
    }
  }
  if (connected_) {
    int rc = mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS) {
      std::cerr << "Unsubscribe error: " << mosquitto_strerror(rc) << std::endl;
      return false;
    }
  }
  return true;
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retain)
{
  if (!mosq_ || !connected_) return false;
  int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), payload.size(), payload.c_str(), qos, retain);
  if (rc != MOSQ_ERR_SUCCESS) {
    std::cerr << "Publish error: " << mosquitto_strerror(rc) << std::endl;
    return false;
  }
  return true;
}

bool MqttClient::isConnected() const { return connected_; }

void MqttClient::run()
{
  running_ = true;
  while (running_) {
    if (!connected_) {
      std::cerr << "Disconnected, attempting reconnect..." << std::endl;
      if (reconnect()) {
        std::cout << "Reconnected successfully." << std::endl;
        resubscribe();
      } else {
        std::cerr << "Reconnect failed, retrying in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void MqttClient::stop()
{
  running_ = false;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool MqttClient::reconnect()
{
  if (!mosq_) return false;
  // 断开现有连接（如果有）
  mosquitto_disconnect(mosq_);
  // 重新连接
  int rc = mosquitto_connect(mosq_, host_.c_str(), port_, 60);
  if (rc != MOSQ_ERR_SUCCESS) { return false; }
  // 等待连接成功回调
  for (int i = 0; i < 50 && !connected_; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
  return connected_;
}

void MqttClient::resubscribe()
{
  for (const auto& sub : subscriptions_) {
    int rc = mosquitto_subscribe(mosq_, nullptr, sub.topic.c_str(), sub.qos);
    if (rc != MOSQ_ERR_SUCCESS) {
      std::cerr << "Resubscribe error for topic " << sub.topic << ": " << mosquitto_strerror(rc) << std::endl;
    }
  }
}

// ---------- 静态回调函数实现 ----------
void MqttClient::on_connect(struct mosquitto* mosq, void* obj, int rc)
{
  MqttClient* client = static_cast<MqttClient*>(obj);
  if (rc == 0) {
    client->connected_ = true;
    std::cout << "Connected to MQTT broker." << std::endl;
    if (client->conn_cb_) client->conn_cb_();
  } else {
    client->connected_ = false;
    std::cerr << "Connection failed: " << mosquitto_connack_string(rc) << std::endl;
  }
}

void MqttClient::on_disconnect(struct mosquitto* mosq, void* obj, int rc)
{
  MqttClient* client = static_cast<MqttClient*>(obj);
  client->connected_ = false;
  std::cout << "Disconnected from MQTT broker." << std::endl;
}

void MqttClient::on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
  MqttClient* client = static_cast<MqttClient*>(obj);
  if (client->msg_cb_) {
    std::string topic(msg->topic);
    std::string payload(static_cast<const char*>(msg->payload), msg->payloadlen);
    client->msg_cb_(topic, payload);
  }
}