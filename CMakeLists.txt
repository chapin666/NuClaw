cmake_minimum_required(VERSION 3.14)
project(NuClaw-Step0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找 Boost
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# Step 0: HTTP Echo 服务器
add_executable(nuclaw_step00 src/step00/main.cpp)

target_link_libraries(nuclaw_step00 
    Boost::system 
    Threads::Threads
)

# 安装目标
install(TARGETS nuclaw_step00 RUNTIME DESTINATION bin)

# 打印配置信息
message(STATUS "")
message(STATUS "NuClaw Step 0 Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Boost Version: ${Boost_VERSION}")
message(STATUS "  Executable: nuclaw_step00")
message(STATUS "")
message(STATUS "Build commands:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "")
