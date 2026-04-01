# SQLite 交叉编译


# 指定交叉编译器

## 设置环境变量（关键）

export CC=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
export CXX=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-g++
export AR=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-ar
export AS=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-as
export LD=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-ld
export RANLIB=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-ranlib
export STRIP=/home/lanceli/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-strip

## 配置编译参数

mkdir -p ../sqlite3-arm-install

## 运行 configure 配置（针对 cortex-a7 优化）：

./configure \
  --host=arm-linux-gnueabihf \
  --prefix=$(pwd)/../sqlite3-arm-install \
  --enable-static \
  --enable-shared \
  --disable-tcl \
  --disable-readline \
  CFLAGS="-Os -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fPIC" \
  LDFLAGS="-Wl,-O1 -Wl,--hash-style=gnu"


- *-Os: 优化代码大小；-mcpu=cortex-a7: 针对cortex-a7优化；-mfpu=neon-vfpv4: 启用NEON和VFPv4浮点；-mfloat-abi=hard: 硬浮点ABI；*
- *-fPIC: 生成位置无关代码*

## 编译与安装

make
make install

# APP 编译步骤


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



## 需求（SQLite）

编写一个 SQLite 应用程序，实现将消息结构 ModbusMasterMsg 中的内容写入到数据库中，具体需求如下：
1. SQLite 数据库交互操作类的声明放在 dbManager.h 当中，类函数的定义放在 dbManager.cpp 当中，交互函数至少包含增（写入）、删（删除数据表）、查（根据 id + channel 读取信息）；
   -  在初始化过程中，根据初始化参数的值对数据库进行创建，例如 DataBaseManager db(2) 即为创建 CHANNEL01 和 CHANNEL02两个数据库；
   -  每个channel一个数据路，即判断 channel 的值后，将 channel 值相同的内容写入到对应channel的数据库，譬如有 channel 值有 1和2 两个通道，channel为1的 ModbusMasterMsg 消息结构的 id、pdu_addr、pdu_func、pdu_data、timestamp、register_map 内容写入到名为 CHANNEL01的数据库；
   -  一个数据中包含多张数据表，每张数据表以日期作为区分，单位为天，例如 20260401；
   -  一个数据库中最大的表格数目在初始化过程中可设置，如果超出最大数量的阈值，则创建新表后将最旧一张表格删除，以控制数据库占用的磁盘空间；
   -  数据表的表头分别为：id(ModbusMasterMsg中的id)、pdu_addr(ModbusMasterMsg中的pdu_addr)、pdu_func(ModbusMasterMsg中的pdu_func)、pdu_data(ModbusMasterMsg中的pdu_data)、timestamp(ModbusMasterMsg中的timestamp)、address(ModbusMasterMsg中的register_map.address)、map_addr(ModbusMasterMsg中的register_map.map_addr)、value(ModbusMasterMsg中的register_map.value)、description(ModbusMasterMsg中的register_map.description)；
   -  同一帧报文中的 register_map 数组如果有多个对象，则以每个对象一行的方式分多行写入到数据表当中，这种情况下，register_map 中的 address、map_addr、value、description 可能有不同，其他参数（id、channel、pdu_addr、pdu_func、pdu_data、timestamp）则共用同一个值；
2. 程序编译语言版本为C++11，运行环境为 aarch32 linux；
3. 在 main.cpp 文件中的 main 函数进行数据库初始化操作；
4. 在 main.cpp 文件 main 函数 的定时读取线程中，加入数据库写入函数，将从 ModbusMasterMsg 消息队列中读取的内容写入到 SQLite 数据库；
