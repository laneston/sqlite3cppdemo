# 简单 C++ 项目配置
cmake_minimum_required(VERSION 3.10)
project(SimpleProject VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
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

# 设置动态库搜索路径
set(LIB_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/lib          # 项目内的库
    /usr/lib/x86_64-linux-gnu        # Ubuntu 系统库目录
    /usr/local/lib                   # 用户安装的库
)

# 方法1：使用 find_library 查找动态库
# 这里以 jsoncpp 为例，你可以替换为需要的库名
find_library(SQLite3 
    NAMES sqlite3
    PATHS ${LIB_SEARCH_PATHS}
)

if(SQLite3)
    message(STATUS "找到 sqlite3 库: ${SQLite3}")
else()
    message(WARNING "未找到 sqlite3 库，某些功能可能不可用")
endif()

# 方法2：直接指定项目内的动态库（如果知道确切路径）
set(MY_CUSTOM_LIB ${CMAKE_SOURCE_DIR}/lib/libch347.so)
if(EXISTS ${MY_CUSTOM_LIB})
    message(STATUS "使用项目内库: ${MY_CUSTOM_LIB}")
endif()

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

# 如果有找到的动态库，链接它们
if(SQLite3)
    target_link_libraries(${PROJECT_NAME} ${SQLite3})
elseif(EXISTS ${MY_CUSTOM_LIB})
    target_link_libraries(${PROJECT_NAME} ${MY_CUSTOM_LIB})
endif()

# 添加其他常用库
target_link_libraries(${PROJECT_NAME} 
    pthread      # POSIX 线程库
    dl           # 动态加载库
)

# 设置可执行文件的属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "sqlite3demo"
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