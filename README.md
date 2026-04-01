

mkdir build && cd build

cmake ..
cmake -DCMAKE_TOOLCHAIN_FILE=../aarch32-toolchain.cmake ..


make

## 需求

设计用于存储 JSON 格式的报文内容的消息结构，并编写相应的交互函数，报文内容格式参考文件 modbusMasterMsg.json ，需求如下：
1. 程序编译语言版本为C++11，运行环境为 aarch32 linux；
2. 初始化过程中指定消息队列的大小；
3. 消息队列为环形结构，如果写入的内容数量超出队列剩余空间，则覆盖掉最早的存储节点；
4. 交互函数包含读函数、写函数；
5. 交互函数支持查询空闲队列大小，即没用用于存储的节点数量；
6. 读函数返回值即为单个报文内容消息结构，读出后将此节点置为空闲状态；
7. 写函数形参包括单个报文内容消息结构，写入后将此节点置为忙碌状态；


## 需求++

修改 main.cpp 文件，实现将从 MQTT 总线接收到的 JSON 报文存入 MessageQueue 消息结构当中，并定期读取打印到日志当中，具体需求如下：

1. 订阅主题 modbusMaster/database/data 监测 modbusMaster 发布的消息内容；
2. 将 modbusMaster/database/data 主体的 payload 进行校验，判定是否符合 MessageQueue  消息结构的规范；
3. 如果符合 MessageQueue 消息结构的规范，则存入队列当中，如果不符合则丢弃不处理；
4. 另起一个线程，每隔60秒查询当前队列中的报文数量，并将队列中所有报文打印出来，直至队列为空；



## 需求++
根据上传的 modbus_message_queue.h 文件对 modbus_message_queue.cpp 文件内容进行修改，并对main.cpp文件进行修改以适应新的功能调整，具体需求如下：
1. 在 modbus_message_queue.h 文件的寄存器映射条目 RegisterItem 中，变量 address 存储的是  modbusMasterMsg.json 中的 数组对象 register_map 里的寄存器地址键名，譬如 "16"、"17"、"18"...
2. 在 modbus_message_queue.h 文件的寄存器映射条目 RegisterItem 中，变量 map_addr 存储的是 modbusMasterMsg.json 中的 数组对象 register_map 里的寄存器地址键值，譬如 0、1、2...
3. 在 modbus_message_queue.h 文件的寄存器映射条目 RegisterItem 中，变量 value 存储的是 modbusMasterMsg.json 中的 数组对象 register_map 里的寄存器值，譬如 220、456...
