# NuClaw - 渐进式 C++ AI Agent 网关教程
# 根 CMakeLists.txt - 构建所有步骤

cmake_minimum_required(VERSION 3.14)
project(nuclaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# Step 01: 异步 I/O + CMake
add_executable(nuclaw_step01 src/step01/main.cpp)
target_link_libraries(nuclaw_step01 
    Boost::system 
    Boost::thread
    Threads::Threads
)

# Step 02: HTTP Keep-Alive
add_executable(nuclaw_step02 src/step02/main.cpp)
target_link_libraries(nuclaw_step02 
    Boost::system 
    Boost::thread
    Threads::Threads
)

# 安装目标
install(TARGETS nuclaw_step01 nuclaw_step02 RUNTIME DESTINATION bin)

# 注意：Step 0 使用 g++ 直接编译，不需要 CMake
# cd src/step00 && g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
