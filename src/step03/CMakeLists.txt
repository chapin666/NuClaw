# ============================================================================
# Step 3 CMakeLists.txt - Agent Loop
# ============================================================================
#
# 文件结构：
#   agent_loop.hpp/cpp - Agent 状态机核心
#   ws_session.hpp/cpp - WebSocket 连接管理
#   server.hpp/cpp     - TCP 服务器
#   main.cpp           - 程序入口
#
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step03 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost 1.70 REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)

# 定义源文件列表
set(SOURCES
    main.cpp
    agent_loop.cpp
    ws_session.cpp
    server.cpp
)

# 创建可执行文件
add_executable(nuclaw_step03 ${SOURCES})

# 链接库
target_link_libraries(nuclaw_step03 
    Boost::system 
    Boost::thread
    Boost::json
    Threads::Threads
)

# 添加头文件搜索路径（当前目录）
target_include_directories(nuclaw_step03 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
