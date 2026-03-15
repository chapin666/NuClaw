# ============================================================================
# CMakeLists.txt - Step 19: 高级功能
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw-step19 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)

# 头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# 可执行文件
add_executable(step19_demo src/main.cpp)

target_link_libraries(step19_demo
    Boost::system
    Boost::thread
    Boost::json
    Threads::Threads
)

# 安装目标
install(TARGETS step19_demo DESTINATION bin)

# 打印配置信息
message(STATUS "========================================")
message(STATUS "NuClaw Step 19 Configuration")
message(STATUS "========================================")
