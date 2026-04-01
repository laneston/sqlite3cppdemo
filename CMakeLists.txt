# 简单 C++ 项目配置
cmake_minimum_required(VERSION 3.10)
project(SimpleProject VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# 设置包含目录（包含第三方库头文件）
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# ========== 查找和链接动态库 ==========

# 设置动态库搜索路径（优先使用项目内交叉编译好的库）
set(LIB_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/lib          # 项目内的库（已交叉编译）
)

# 查找 mosquitto 库
find_library(Mosquitto
    NAMES mosquitto
    PATHS ${LIB_SEARCH_PATHS}
    NO_CMAKE_SYSTEM_PATH            # 禁止搜索系统路径，避免误用主机库
)

# 如果 find_library 未找到，尝试直接指定文件路径（处理符号链接可能引起的问题）
if(NOT Mosquitto)
    set(LIB_PATH ${CMAKE_SOURCE_DIR}/lib/libmosquitto.so)
    if(EXISTS ${LIB_PATH})
        set(Mosquitto ${LIB_PATH})
        message(STATUS "使用直接路径库: ${Mosquitto}")
    else()
        message(FATAL_ERROR "未找到 mosquitto 库，请确保 ${CMAKE_SOURCE_DIR}/lib 中包含交叉编译好的 libmosquitto.so")
    endif()
endif()

message(STATUS "找到 mosquitto 库: ${Mosquitto}")


# 查找 sqlite3 库
find_library(SQLite3
    NAMES sqlite3
    PATHS ${LIB_SEARCH_PATHS}
    NO_CMAKE_SYSTEM_PATH            # 禁止搜索系统路径，避免误用主机库
)

# 如果 find_library 未找到，尝试直接指定文件路径（处理符号链接可能引起的问题）
if(NOT SQLite3)
    set(LIB_PATH ${CMAKE_SOURCE_DIR}/lib/libsqlite3.so)
    if(EXISTS ${LIB_PATH})
        set(SQLite3 ${LIB_PATH})
        message(STATUS "使用直接路径库: ${SQLite3}")
    else()
        message(FATAL_ERROR "未找到 sqlite3 库，请确保 ${CMAKE_SOURCE_DIR}/lib 中包含交叉编译好的 libsqlite3.so")
    endif()
endif()

message(STATUS "找到 sqlite3 库: ${SQLite3}")
# ========== 添加源文件 ==========

# 收集所有源代码文件
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*/*.cpp
)

# 显示找到的源文件
foreach(source ${SOURCE_FILES})
    message(STATUS "源文件: ${source}")
endforeach()

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# ========== 链接库文件 ==========

# 链接 mosquitto 库
target_link_libraries(${PROJECT_NAME} ${Mosquitto})
# 链接 mosquitto 库
target_link_libraries(${PROJECT_NAME} ${SQLite3})

# 添加其他常用库（交叉编译工具链需提供 pthread 和 dl）
target_link_libraries(${PROJECT_NAME}
    pthread      # POSIX 线程库
    dl           # 动态加载库
)

# 设置可执行文件的属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "database"
)

# 安装规则（可选）
install(TARGETS ${PROJECT_NAME}
    DESTINATION bin
)

# 显示最终信息
message(STATUS "=========================================")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "构建目录: ${CMAKE_BINARY_DIR}")
message(STATUS "输出目录: ${EXECUTABLE_OUTPUT_PATH}")
message(STATUS "=========================================")